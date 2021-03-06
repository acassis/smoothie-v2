#include "CommandShell.h"
#include "OutputStream.h"
#include "Dispatcher.h"
#include "Module.h"
#include "StringUtils.h"
#include "Robot.h"

#include <functional>
#include <set>

#include <sys/types.h>
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>

#define HELP(m) if(params == "-h") { os.printf("%s\n", m); return true; }

CommandShell::CommandShell()
{
    mounted = false;
}

bool CommandShell::initialize()
{
    // register command handlers
    using std::placeholders::_1;
    using std::placeholders::_2;

    THEDISPATCHER->add_handler( "help", std::bind( &CommandShell::help_cmd, this, _1, _2) );
    THEDISPATCHER->add_handler( "ls", std::bind( &CommandShell::ls_cmd, this, _1, _2) );
    THEDISPATCHER->add_handler( "rm", std::bind( &CommandShell::rm_cmd, this, _1, _2) );
    THEDISPATCHER->add_handler( "mem", std::bind( &CommandShell::mem_cmd, this, _1, _2) );
    THEDISPATCHER->add_handler( "mount", std::bind( &CommandShell::mount_cmd, this, _1, _2) );
    THEDISPATCHER->add_handler( "cat", std::bind( &CommandShell::cat_cmd, this, _1, _2) );
    THEDISPATCHER->add_handler( "md5sum", std::bind( &CommandShell::md5sum_cmd, this, _1, _2) );
    THEDISPATCHER->add_handler( "switch", std::bind( &CommandShell::switch_cmd, this, _1, _2) );
    THEDISPATCHER->add_handler( "gpio", std::bind( &CommandShell::gpio_cmd, this, _1, _2) );
    THEDISPATCHER->add_handler( "modules", std::bind( &CommandShell::modules_cmd, this, _1, _2) );
    THEDISPATCHER->add_handler( "get", std::bind( &CommandShell::get_cmd, this, _1, _2) );
    THEDISPATCHER->add_handler( "$#", std::bind( &CommandShell::grblDP_cmd, this, _1, _2) );

    return true;
}

// lists all the registered commands
bool CommandShell::help_cmd(std::string& params, OutputStream& os)
{
    HELP("Show available commands");
    auto cmds= THEDISPATCHER->get_commands();
    for(auto& i : cmds) {
        os.printf("%s\n", i.c_str());
        // Display the help string for each command
        //    std::string cmd(i);
        //    cmd.append(" -h");
        //    THEDISPATCHER->dispatch(cmd.c_str(), os);
    }
    os.puts("\nuse cmd -h to get help on that command\n");

    return true;
}

bool CommandShell::ls_cmd(std::string& params, OutputStream& os)
{
    HELP("list files: -s show size");
    std::string path, opts;
    while(!params.empty()) {
        std::string s = stringutils::shift_parameter( params );
        if(s.front() == '-') {
            opts.append(s);
        } else {
            path = s;
            if(!params.empty()) {
                path.append(" ");
                path.append(params);
            }
            break;
        }
    }

    DIR *d;
    struct dirent *p;
    d = opendir(path.c_str());
    if (d != NULL) {
        while ((p = readdir(d)) != NULL) {
            os.printf("%s", p->d_name);
            struct stat buf;
            std::string sp = path + "/" + p->d_name;
            if (stat(sp.c_str(), &buf) >= 0) {
                if (S_ISDIR(buf.st_mode)) {
                    os.printf("/");

                } else if(opts.find("-s", 0, 2) != std::string::npos) {
                    os.printf(" %d", buf.st_size);
                }
            } else {
                os.printf(" - Could not stat: %s", sp.c_str());
            }
            os.printf("\n");
        }
        closedir(d);
    } else {
        os.printf("Could not open directory %s\n", path.c_str());
    }

    return true;
}

bool CommandShell::rm_cmd(std::string& params, OutputStream& os)
{
    HELP("delete file");
    std::string fn = stringutils::shift_parameter( params );
    int s = remove(fn.c_str());
    if (s != 0) os.printf("Could not delete %s\n", fn.c_str());
    return true;
}

bool CommandShell::mem_cmd(std::string& params, OutputStream& os)
{
    HELP("show memory allocation");
    struct mallinfo mem = mallinfo();
    os.printf("             total       used       free    largest\n");
    os.printf("Mem:   %11d%11d%11d%11d\n", mem.arena, mem.uordblks, mem.fordblks, mem.mxordblk);
    return true;
}

#include <sys/mount.h>
#include <sys/boardctl.h>
bool CommandShell::mount_cmd(std::string& params, OutputStream& os)
{
    HELP("mount sdcard on /sd (or unmount if already mounted)");

    const char g_target[]         = "/sd";
    const char g_filesystemtype[] = "vfat";
    const char g_source[]         = "/dev/mmcsd0";

    if(mounted) {
        os.printf("Already mounted, unmounting\n");
        umount(g_target);
        mounted= false;
        return true;
    }

    int ret;
    // ret = boardctl(BOARDIOC_INIT, 0);
    // if(OK != ret) {
    //     os.printf("Failed to INIT SDIO\n");
    //     return true;
    // }

    ret = mount(g_source, g_target, g_filesystemtype, 0, nullptr);
    if(0 == ret) {
        mounted = true;
        os.printf("Mounted %s on %s\n", g_source, g_target);

    }else{
        os.printf("Failed to mount sdcard\n");
    }
    return true;
}

bool CommandShell::cat_cmd(std::string& params, OutputStream& os)
{
    HELP("display file: nnn option will show first nnn lines");
    // Get params ( filename and line limit )
    std::string filename          = stringutils::shift_parameter( params );
    std::string limit_parameter   = stringutils::shift_parameter( params );
    int limit = -1;

    if ( limit_parameter != "" ) {
        char *e = NULL;
        limit = strtol(limit_parameter.c_str(), &e, 10);
        if (e <= limit_parameter.c_str())
            limit = -1;
    }

    // Open file
    FILE *lp = fopen(filename.c_str(), "r");
    if (lp != NULL) {
        char buffer[132];
        int newlines = 0;
        // Print each line of the file
        while (fgets (buffer, sizeof(buffer)-1, lp) != nullptr) {
            os.puts(buffer);
            if ( limit > 0 && ++newlines >= limit ) {
                break;
            }
        };
        fclose(lp);

    }else{
        os.printf("File not found: %s\n", filename.c_str());
    }

    return true;
}

#include "md5.h"
bool CommandShell::md5sum_cmd(std::string& params, OutputStream& os)
{
    HELP("calculate the md5sum of given filename");

    // Open file
    FILE *lp = fopen(params.c_str(), "r");
    if (lp != NULL) {
        MD5 md5;
        uint8_t buf[64];
        do {
            size_t n= fread(buf, 1, sizeof buf, lp);
            if(n > 0) md5.update(buf, n);
        } while(!feof(lp));

        os.printf("%s %s\n", md5.finalize().hexdigest().c_str(), params.c_str());
        fclose(lp);

    }else{
        os.printf("File not found: %s\n", params.c_str());
    }

    return true;
}

#include "Switch.h"
// set or get switch state for a named switch
bool CommandShell::switch_cmd(std::string& params, OutputStream& os)
{
    HELP("list switches or get/set named switch. if 2nd parameter is on/off it sets state if it is numeric it sets value");

    std::string name = stringutils::shift_parameter( params );
    std::string value = stringutils::shift_parameter( params );

    if(name.empty()) {
        // just list all the switches
        std::vector<Module*> mv = Module::lookup_group("switch");
        if(mv.size() > 0) {
            for(auto m : mv) {
                Switch *s= static_cast<Switch*>(m);
                os.printf("%s:\n", m->get_instance_name());
                os.printf(" %s\n", s->get_info().c_str());
            }
        }else{
            os.printf("No switches found\n");
        }

        return true;
    }

    Module *m= Module::lookup("switch", name.c_str());
    if(m == nullptr) {
        os.printf("no such switch: %s\n", name.c_str());
        return true;
    }

    bool ok = false;
    if(value.empty()) {
        // get switch state
        bool state;
        ok= m->request("state", &state);
        if (!ok) {
            os.printf("unknown command %s.\n", "state");
            return true;
        }
        os.printf("switch %s is %d\n", name.c_str(), state);

    }else{
        const char *cmd;
        // set switch state
        if(value == "on" || value == "off") {
            bool b = value == "on";
            cmd= "set-state";
            ok =  m->request(cmd, &b);

        } else {
            float v = strtof(value.c_str(), NULL);
            cmd= "set-value";
            ok = m->request(cmd, &v);
        }

        if (ok) {
            os.printf("switch %s set to: %s\n", name.c_str(), value.c_str());
        } else {
            os.printf("unknown command %s.\n", cmd);
        }
    }

    return true;
}

// set or get gpio
bool CommandShell::gpio_cmd(std::string& params, OutputStream& os)
{
    HELP("set and get gpio pins: use GPIO5[14] | gpio5_14 | P4_10 | p4.10 out/in [on/off]");

    std::string gpio = stringutils::shift_parameter( params );
    std::string dir = stringutils::shift_parameter( params );

    if(gpio.empty()) return false;

    if(dir.empty() || dir == "in") {
        // read pin
        Pin pin(gpio.c_str(), Pin::AS_INPUT);
        if(!pin.connected()) {
            os.printf("Not a valid GPIO\n");
            return true;
        }

        os.printf("%s: %d\n", pin.to_string().c_str(), pin.get());
        return true;
    }

    if(dir == "out") {
        std::string v = stringutils::shift_parameter( params );
        if(v.empty()) return false;
        Pin pin(gpio.c_str(), Pin::AS_OUTPUT);
        if(!pin.connected()) {
            os.printf("Not a valid GPIO\n");
            return true;
        }
        bool b= (v == "on");
        pin.set(b);
        os.printf("%s: set to %d\n", pin.to_string().c_str(), pin.get());
        return true;
    }

    return false;
}

bool CommandShell::modules_cmd(std::string& params, OutputStream& os)
{
    HELP("List all registered modules\n");

    std::vector<std::string> l= Module::print_modules();

    if(l.empty()) {
        os.printf("No modules found\n");
        return true;
    }

    for(auto& i : l) {
        os.printf("%s\n", i.c_str());
    }

    return true;
}

bool CommandShell::get_cmd(std::string& params, OutputStream& os)
{
    HELP("get pos|wcs|state")
    std::string what = stringutils::shift_parameter( params );
    bool handled= true;
    if (what == "temp") {
        // struct pad_temperature temp;
        // string type = shift_parameter( params );
        // if(type.empty()) {
        //     // scan all temperature controls
        //     std::vector<struct pad_temperature> controllers;
        //     bool ok = PublicData::get_value(temperature_control_checksum, poll_controls_checksum, &controllers);
        //     if (ok) {
        //         for (auto &c : controllers) {
        //            os.printf("%s (%d) temp: %f/%f @%d\r\n", c.designator.c_str(), c.id, c.current_temperature, c.target_temperature, c.pwm);
        //         }

        //     } else {
        //         os.printf("no heaters found\r\n");
        //     }

        // }else{
        //     bool ok = PublicData::get_value( temperature_control_checksum, current_temperature_checksum, get_checksum(type), &temp );

        //     if (ok) {
        //         os.printf("%s temp: %f/%f @%d\r\n", type.c_str(), temp.current_temperature, temp.target_temperature, temp.pwm);
        //     } else {
        //         os.printf("%s is not a known temperature device\r\n", type.c_str());
        //     }
        // }

    } else if (what == "fk" || what == "ik") {
        // string p= shift_parameter( params );
        // bool move= false;
        // if(p == "-m") {
        //     move= true;
        //     p= shift_parameter( params );
        // }

        // std::vector<float> v= parse_number_list(p.c_str());
        // if(p.empty() || v.size() < 1) {
        //     os.printf("error:usage: get [fk|ik] [-m] x[,y,z]\n");
        //     return;
        // }

        // float x= v[0];
        // float y= (v.size() > 1) ? v[1] : x;
        // float z= (v.size() > 2) ? v[2] : y;

        // if(what == "fk") {
        //     // do forward kinematics on the given actuator position and display the cartesian coordinates
        //     ActuatorCoordinates apos{x, y, z};
        //     float pos[3];
        //     Robot::getInstance()->arm_solution->actuator_to_cartesian(apos, pos);
        //     os.printf("cartesian= X %f, Y %f, Z %f\n", pos[0], pos[1], pos[2]);
        //     x= pos[0];
        //     y= pos[1];
        //     z= pos[2];

        // }else{
        //     // do inverse kinematics on the given cartesian position and display the actuator coordinates
        //     float pos[3]{x, y, z};
        //     ActuatorCoordinates apos;
        //     Robot::getInstance()->arm_solution->cartesian_to_actuator(pos, apos);
        //     os.printf("actuator= X %f, Y %f, Z %f\n", apos[0], apos[1], apos[2]);
        // }

        // if(move) {
        //     // move to the calculated, or given, XYZ
        //     char cmd[64];
        //     snprintf(cmd, sizeof(cmd), "G53 G0 X%f Y%f Z%f", x, y, z);
        //     struct SerialMessage message;
        //     message.message = cmd;
        //     message.stream = &(StreamOutput::NullStream);
        //     THEKERNEL->call_event(ON_CONSOLE_LINE_RECEIVED, &message );
        //     THECONVEYOR->wait_for_idle();
        // }

   } else if (what == "pos") {
        // convenience to call all the various M114 variants, shows ABC axis where relevant
        std::string buf;
        Robot::getInstance()->print_position(0, buf); os.printf("last %s\n", buf.c_str()); buf.clear();
        Robot::getInstance()->print_position(1, buf); os.printf("realtime %s\n", buf.c_str()); buf.clear();
        Robot::getInstance()->print_position(2, buf); os.printf("%s\n", buf.c_str()); buf.clear();
        Robot::getInstance()->print_position(3, buf); os.printf("%s\n", buf.c_str()); buf.clear();
        Robot::getInstance()->print_position(4, buf); os.printf("%s\n", buf.c_str()); buf.clear();
        Robot::getInstance()->print_position(5, buf); os.printf("%s\n", buf.c_str()); buf.clear();

    } else if (what == "wcs") {
        // print the wcs state
        std::string cmd("-v");
        grblDP_cmd(cmd, os);

    } else if (what == "state") {
        // also $G
        // [G0 G54 G17 G21 G90 G94 M0 M5 M9 T0 F0.]
        os.printf("[G%d %s G%d G%d G%d G94 M0 M5 M9 T%d F%1.4f S%1.4f]\n",
            1, // Dispatcher.getInstance()->get_modal_command(),
            stringutils::wcs2gcode(Robot::getInstance()->get_current_wcs()).c_str(),
            Robot::getInstance()->plane_axis_0 == X_AXIS && Robot::getInstance()->plane_axis_1 == Y_AXIS && Robot::getInstance()->plane_axis_2 == Z_AXIS ? 17 :
              Robot::getInstance()->plane_axis_0 == X_AXIS && Robot::getInstance()->plane_axis_1 == Z_AXIS && Robot::getInstance()->plane_axis_2 == Y_AXIS ? 18 :
              Robot::getInstance()->plane_axis_0 == Y_AXIS && Robot::getInstance()->plane_axis_1 == Z_AXIS && Robot::getInstance()->plane_axis_2 == X_AXIS ? 19 : 17,
            Robot::getInstance()->inch_mode ? 20 : 21,
            Robot::getInstance()->absolute_mode ? 90 : 91,
            0, //get_active_tool(),
            Robot::getInstance()->from_millimeters(Robot::getInstance()->get_feed_rate()),
            Robot::getInstance()->get_s_value());

    } else if (what == "status") {
        // also ? on serial and usb
        //os.printf("%s\n", THEKERNEL->get_query_string().c_str());

    } else {

        handled= false;
    }

    return handled;
}

bool CommandShell::grblDP_cmd(std::string& params, OutputStream& os)
{
    /*
    [G54:95.000,40.000,-23.600]
    [G55:0.000,0.000,0.000]
    [G56:0.000,0.000,0.000]
    [G57:0.000,0.000,0.000]
    [G58:0.000,0.000,0.000]
    [G59:0.000,0.000,0.000]
    [G28:0.000,0.000,0.000]
    [G30:0.000,0.000,0.000]
    [G92:0.000,0.000,0.000]
    [TLO:0.000]
    [PRB:0.000,0.000,0.000:0]
    */

    HELP("show grbl $ command")

    bool verbose = stringutils::shift_parameter( params ).find_first_of("Vv") != std::string::npos;

    std::vector<Robot::wcs_t> v= Robot::getInstance()->get_wcs_state();
    if(verbose) {
        char current_wcs= std::get<0>(v[0]);
        os.printf("[current WCS: %s]\n", stringutils::wcs2gcode(current_wcs).c_str());
    }

    int n= std::get<1>(v[0]);
    for (int i = 1; i <= n; ++i) {
        os.printf("[%s:%1.4f,%1.4f,%1.4f]\n", stringutils::wcs2gcode(i-1).c_str(),
            Robot::getInstance()->from_millimeters(std::get<0>(v[i])),
            Robot::getInstance()->from_millimeters(std::get<1>(v[i])),
            Robot::getInstance()->from_millimeters(std::get<2>(v[i])));
    }

    float rd[]{0,0,0};
    //PublicData::get_value( endstops_checksum, saved_position_checksum, &rd ); TODO use request
    os.printf("[G28:%1.4f,%1.4f,%1.4f]\n",
        Robot::getInstance()->from_millimeters(rd[0]),
        Robot::getInstance()->from_millimeters(rd[1]),
        Robot::getInstance()->from_millimeters(rd[2]));

    os.printf("[G30:%1.4f,%1.4f,%1.4f]\n",  0.0F, 0.0F, 0.0F); // not implemented

    os.printf("[G92:%1.4f,%1.4f,%1.4f]\n",
        Robot::getInstance()->from_millimeters(std::get<0>(v[n+1])),
        Robot::getInstance()->from_millimeters(std::get<1>(v[n+1])),
        Robot::getInstance()->from_millimeters(std::get<2>(v[n+1])));

    if(verbose) {
        os.printf("[Tool Offset:%1.4f,%1.4f,%1.4f]\n",
            Robot::getInstance()->from_millimeters(std::get<0>(v[n+2])),
            Robot::getInstance()->from_millimeters(std::get<1>(v[n+2])),
            Robot::getInstance()->from_millimeters(std::get<2>(v[n+2])));
    }else{
        os.printf("[TL0:%1.4f]\n", Robot::getInstance()->from_millimeters(std::get<2>(v[n+2])));
    }

    // this is the last probe position, updated when a probe completes, also stores the number of steps moved after a homing cycle
    float px, py, pz;
    uint8_t ps;
    std::tie(px, py, pz, ps) = Robot::getInstance()->get_last_probe_position();
    os.printf("[PRB:%1.4f,%1.4f,%1.4f:%d]\n", Robot::getInstance()->from_millimeters(px), Robot::getInstance()->from_millimeters(py), Robot::getInstance()->from_millimeters(pz), ps);

    return true;
}

