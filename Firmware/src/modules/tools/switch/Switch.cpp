#if 0
#include "Switch.h"

#include "GCode.h"
#include "OutputStream.h"
#include "ConfigReader.h"
#include "SlowTicker.h"


//#include "PwmOut.h"

#include <algorithm>
#include <math.h>

#define startup_state_key       "startup_state"
#define startup_value_key       "startup_value"
#define input_pin_key           "input_pin"
#define input_pin_behavior_key  "input_pin_behavior"
#define toggle_key              "toggle"
#define momentary_key           "momentary"
#define command_subcode_key     "subcode"
#define input_on_command_key    "input_on_command"
#define input_off_command_key   "input_off_command"
#define output_pin_key          "output_pin"
#define output_type_key         "output_type"
#define max_pwm_key             "max_pwm"
#define output_on_command_key   "output_on_command"
#define output_off_command_key  "output_off_command"
#define pwm_period_ms_key       "pwm_period_ms"
#define failsafe_key            "failsafe_set_to"
#define ignore_onhalt_key       "ignore_on_halt"

Switch::Switch(const char *name) : Module("switch", name)
{ }

// this is called by the intial startup
bool Switch::configure(ConfigReader& cr)
{
    return load_switches(cr);
}

bool Switch::load_switches(ConfigReader& cr)
{
    ConfigReader::sub_section_map_t ssmap;
    if(!cr.get_sub_sections("switch", ssmap)) {
        printf("configure-switch: no switch section found\n");
        return false;
    }

    for(auto& i : ssmap) {
        // foreach switch
        std::string name = i.first;
        auto& m = i.second;
        Switch *sw = new Switch(name.c_str());
        sw->configure(cr, m);
    }
}

bool Switch::configure(ConfigReader& cr, ConfigReader::section_map_t& m)
{
    this->input_pin.from_string( cr.get_string(m, input_pin_key, "nc") )->as_input();
    this->subcode = cr.get_int(m, command_subcode_key, 0);
    std::string input_on_command = cr.get_string(m, input_on_command_key, "");
    std::string input_off_command = cr.get_string(m, input_off_command_key, "");
    this->output_on_command = cr.get_string(m, output_on_command_key, "");
    this->output_off_command = cr.get_string(m, output_off_command_key, "");

    this->switch_state = cr.get_bool(m, startup_state_key, false);
    std::string type = cr.get_string(m, output_type_key, "pwm");
    this->failsafe = cr.get_int(m, failsafe_key, 0);
    this->ignore_on_halt = cr.get_bool(m, ignore_onhalt_key, false);

    std::string ipb = cr.get_string(m, input_pin_behavior_key, "momentary");
    this->input_pin_behavior = (ipb == "momentary") ? momentary_behavior : toggle_behavior;

    if(type == "pwm") {
        this->output_type = SIGMADELTA;
        // this->sigmadelta_pin = new Pwm();
        // this->sigmadelta_pin->from_string(cr.get_string(m, output_pin_key, "nc"))->as_output();
        // if(this->sigmadelta_pin->connected()) {
        //     if(failsafe == 1) {
        //         set_high_on_debug(sigmadelta_pin->port_number, sigmadelta_pin->pin);
        //     } else {
        //         set_low_on_debug(sigmadelta_pin->port_number, sigmadelta_pin->pin);
        //     }
        // } else {
        //     this->output_type = NONE;
        //     delete this->sigmadelta_pin;
        //     this->sigmadelta_pin = nullptr;
        // }

    } else if(type == "digital") {
        this->output_type = DIGITAL;
        this->digital_pin = new Pin();
        this->digital_pin->from_string(cr.get_string(m, output_pin_key, "nc"))->as_output();
        if(this->digital_pin->connected()) {
            if(failsafe == 1) {
                //set_high_on_debug(digital_pin->port_number, digital_pin->pin);
            } else {
                //set_low_on_debug(digital_pin->port_number, digital_pin->pin);
            }
        } else {
            this->output_type = NONE;
            delete this->digital_pin;
            this->digital_pin = nullptr;
        }

    } else if(type == "hwpwm") {
        this->output_type = HWPWM;
        // Pin *pin = new Pin();
        // pin->from_string(cr.get_string(m, output_pin_key, "nc"))->as_output();
        // this->pwm_pin = pin->hardware_pwm();
        // if(failsafe == 1) {
        //     set_high_on_debug(pin->port_number, pin->pin);
        // } else {
        //     set_low_on_debug(pin->port_number, pin->pin);
        // }
        // delete pin;
        // if(this->pwm_pin == nullptr) {
        //     THEKERNEL->streams->printf("Selected Switch output pin is not PWM capable - disabled");
        //     this->output_type = NONE;
        // }

    } else {
        this->output_type = NONE;
    }

    if(this->output_type == SIGMADELTA) {
        // this->sigmadelta_pin->max_pwm(cr.get_int(m,  max_pwm_key, 255));
        // this->switch_value = cr.get_int(m, startup_value_key, this->sigmadelta_pin->max_pwm());
        // if(this->switch_state) {
        //     this->sigmadelta_pin->pwm(this->switch_value); // will be truncated to max_pwm
        // } else {
        //     this->sigmadelta_pin->set(false);
        // }

    } else if(this->output_type == HWPWM) {
        // default is 50Hz
        // float p = cr.get_float(m, pwm_period_ms_key, 20) * 1000.0F; // ms but fractions are allowed
        // this->pwm_pin->period_us(p);

        // // default is 0% duty cycle
        // this->switch_value = cr.get_int(m, startup_value_key, 0);
        // if(this->switch_state) {
        //     this->pwm_pin->write(this->switch_value / 100.0F);
        // } else {
        //     this->pwm_pin->write(0);
        // }

    } else if(this->output_type == DIGITAL) {
        this->digital_pin->set(this->switch_state);
    }

    // Set the on/off command codes, Use GCode to do the parsing
    input_on_command_letter = 0;
    input_off_command_letter = 0;

    if(!input_on_command.empty()) {
        GCode gc(input_on_command, NULL);
        if(gc.has_g) {
            input_on_command_letter = 'G';
            input_on_command_code = gc.g;
        } else if(gc.has_m) {
            input_on_command_letter = 'M';
            input_on_command_code = gc.m;
        }
    }
    if(!input_off_command.empty()) {
        GCode gc(input_off_command, NULL);
        if(gc.has_g) {
            input_off_command_letter = 'G';
            input_off_command_code = gc.g;
        } else if(gc.has_m) {
            input_off_command_letter = 'M';
            input_off_command_code = gc.m;
        }
    }

    if(input_pin.connected()) {
        // set to initial state
        this->input_pin_state = this->input_pin.get();
        // input pin polling
        SlowTicker.getInstance().attach(100, std::bind(&Switch::pinpoll_tick, this));
    }

    if(this->output_type == SIGMADELTA) {
        // SIGMADELTA
        //SlowTicker.getInstance().attach(1000, std::bind(&Pwm::on_tick, this->sigmadelta_pin));
    }

    // for commands we may need to replace _ for space for old configs
    std::replace(output_on_command.begin(), output_on_command.end(), '_', ' '); // replace _ with space
    std::replace(output_off_command.begin(), output_off_command.end(), '_', ' '); // replace _ with space
}

// set the pin to the fail safe value on halt
void Switch::on_halt(bool flg)
{
    if(flg) {
        if(this->ignore_on_halt) return;

        // set pin to failsafe value
        switch(this->output_type) {
            case DIGITAL: this->digital_pin->set(this->failsafe); break;
            // case SIGMADELTA: this->sigmadelta_pin->set(this->failsafe); break;
            // case HWPWM: this->pwm_pin->write(0); break;
            case NONE: break;
        }
        this->switch_state = this->failsafe;
    }
}

bool Switch::match_input_on_gcode(const GCode& gcode) const
{
    bool b = ((input_on_command_letter == 'M' && gcode.has_m && gcode.m == input_on_command_code) ||
              (input_on_command_letter == 'G' && gcode.has_g && gcode.g == input_on_command_code));

    return (b && gcode.subcode == this->subcode);
}

bool Switch::match_input_off_gcode(const GCode& gcode) const
{
    bool b = ((input_off_command_letter == 'M' && gcode.has_m && gcode.m == input_off_command_code) ||
              (input_off_command_letter == 'G' && gcode.has_g && gcode.g == input_off_command_code));
    return (b && gcode.subcode == this->subcode);
}

void Switch::on_gcode_received(GCode& gcode)
{
    // Add the gcode to the queue ourselves if we need it
    if (!(match_input_on_gcode(gcode) || match_input_off_gcode(gcode))) {
        return;
    }

    // we need to sync this with the queue, so we need to wait for queue to empty, however due to certain slicers
    // issuing redundant swicth on calls regularly we need to optimize by making sure the value is actually changing
    // hence we need to do the wait for queue in each case rather than just once at the start
    if(match_input_on_gcode(gcode)) {
        if (this->output_type == SIGMADELTA) {
            // SIGMADELTA output pin turn on (or off if S0)
            if(gcode.has_letter('S')) {
                int v = roundf(gcode.get_value('S') * sigmadelta_pin->max_pwm() / 255.0F); // scale by max_pwm so input of 255 and max_pwm of 128 would set value to 128
                if(v != this->sigmadelta_pin->get_pwm()) { // optimize... ignore if already set to the same pwm
                    // drain queue
                    THEKERNEL->conveyor->wait_for_idle();
                    this->sigmadelta_pin->pwm(v);
                    this->switch_state = (v > 0);
                }
            } else {
                // drain queue
                THEKERNEL->conveyor->wait_for_idle();
                this->sigmadelta_pin->pwm(this->switch_value);
                this->switch_state = (this->switch_value > 0);
            }

        } else if (this->output_type == HWPWM) {
            // drain queue
            THEKERNEL->conveyor->wait_for_idle();
            // PWM output pin set duty cycle 0 - 100
            if(gcode.has_letter('S')) {
                float v = gcode.get_value('S');
                if(v > 100) v = 100;
                else if(v < 0) v = 0;
                this->pwm_pin->write(v / 100.0F);
                this->switch_state = (v != 0);
            } else {
                this->pwm_pin->write(this->switch_value);
                this->switch_state = (this->switch_value != 0);
            }

        } else if (this->output_type == DIGITAL) {
            // drain queue
            THEKERNEL->conveyor->wait_for_idle();
            // logic pin turn on
            this->digital_pin->set(true);
            this->switch_state = true;
        }

    } else if(match_input_off_gcode(gcode)) {
        // drain queue
        THEKERNEL->conveyor->wait_for_idle();
        this->switch_state = false;
        if (this->output_type == SIGMADELTA) {
            // SIGMADELTA output pin
            this->sigmadelta_pin->set(false);

        } else if (this->output_type == HWPWM) {
            this->pwm_pin->write(0);

        } else if (this->output_type == DIGITAL) {
            // logic pin turn off
            this->digital_pin->set(false);
        }
    }
}

bool Switch::request(const char *key, void *value)
{
    if(strcmp(key, "state") == 0) {
        *(bool *)value = this->switch_state;

    } else if(strcmp(key, "set-state") == 0) {
        this->switch_state = (bool)value;
        switch_changed();

        // if there is no gcode to be sent then we can do this now (in on_idle)
        // Allows temperature switch to turn on a fan even if main loop is blocked with heat and wait
        //if(this->output_on_command.empty() && this->output_off_command.empty()) on_main_loop(nullptr);

    } else if(strcmp(key, "value") == 0) {
        *(float *)value = this->switch_value;

    } else if(strcmp(key, "set-value") == 0) {
        this->switch_value = (float)value;
        switch_changed();

    } else {
        return false;
    }

    return true;
}

void Switch::switch_changed()
{
    if(this->switch_state) {
        if(!this->output_on_command.empty()) this->send_gcode( this->output_on_command, &(StreamOutput::NullStream) );

        if(this->output_type == SIGMADELTA) {
            this->sigmadelta_pin->pwm(this->switch_value); // this requires the value has been set otherwise it switches on to whatever it last was

        } else if (this->output_type == HWPWM) {
            this->pwm_pin->write(this->switch_value / 100.0F);

        } else if (this->output_type == DIGITAL) {
            this->digital_pin->set(true);
        }

    } else {

        if(!this->output_off_command.empty()) this->send_gcode( this->output_off_command, &(StreamOutput::NullStream) );

        if(this->output_type == SIGMADELTA) {
            this->sigmadelta_pin->set(false);

        } else if (this->output_type == HWPWM) {
            this->pwm_pin->write(0);

        } else if (this->output_type == DIGITAL) {
            this->digital_pin->set(false);
        }
    }
}

// Check the state of the button and act accordingly
uint32_t Switch::pinpoll_tick(uint32_t dummy)
{
    if(!input_pin.connected()) return 0;

    // If pin changed
    bool current_state = this->input_pin.get();
    if(this->input_pin_state != current_state) {
        this->input_pin_state = current_state;
        // If pin high
        if( this->input_pin_state ) {
            // if switch is a toggle switch
            if( this->input_pin_behavior == toggle_behavior ) {
                this->flip();
            } else {
                // else default is momentary
                this->switch_state = this->input_pin_state;
                this->switch_changed = true;
            }

        } else {
            // else if button released
            if( this->input_pin_behavior == momentary_behavior ) {
                // if switch is momentary
                this->switch_state = this->input_pin_state;
                this->switch_changed = true;
            }
        }
    }
    return 0;
}

void Switch::flip()
{
    this->switch_state = !this->switch_state;
    this->switch_changed = true;
}

void Switch::send_gcode(std::string msg)
{
    struct SerialMessage message;
    message.message = msg;
    message.stream = stream;
    THEKERNEL->call_event(ON_CONSOLE_LINE_RECEIVED, &message );
}

#endif