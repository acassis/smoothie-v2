#include "CommandShell.h"
#include "OutputStream.h"
#include "Dispatcher.h"

#include <functional>

#include <sys/types.h>
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>

CommandShell::CommandShell()
{
    mounted = false;
}

bool CommandShell::initialize()
{
    // register command handlers
    using std::placeholders::_1;
    using std::placeholders::_2;

    THEDISPATCHER.add_handler( "ls", std::bind( &CommandShell::ls_cmd, this, _1, _2) );
    THEDISPATCHER.add_handler( "rm", std::bind( &CommandShell::rm_cmd, this, _1, _2) );
    THEDISPATCHER.add_handler( "mem", std::bind( &CommandShell::mem_cmd, this, _1, _2) );
    THEDISPATCHER.add_handler( "mount", std::bind( &CommandShell::mount_cmd, this, _1, _2) );
    THEDISPATCHER.add_handler( "cat", std::bind( &CommandShell::cat_cmd, this, _1, _2) );

    return true;
}

// Get the first parameter, and remove it from the original string
// TODO move to utils
std::string shift_parameter( std::string &parameters )
{
    size_t beginning = parameters.find_first_of(" ");
    if( beginning == std::string::npos ) {
        std::string temp = parameters;
        parameters = "";
        return temp;
    }
    std::string temp = parameters.substr( 0, beginning );
    parameters = parameters.substr(beginning + 1, parameters.size());
    return temp;
}

bool CommandShell::ls_cmd(std::string& params, OutputStream& os)
{
    std::string path, opts;
    while(!params.empty()) {
        std::string s = shift_parameter( params );
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
    std::string fn = shift_parameter( params );
    int s = remove(fn.c_str());
    if (s != 0) os.printf("Could not delete %s\n", fn.c_str());
    return true;
}

bool CommandShell::mem_cmd(std::string& params, OutputStream& os)
{
    struct mallinfo mem = mallinfo();
    os.printf("             total       used       free    largest\n");
    os.printf("Mem:   %11d%11d%11d%11d\n", mem.arena, mem.uordblks, mem.fordblks, mem.mxordblk);
    return true;
}

#include <sys/mount.h>
#include <sys/boardctl.h>
bool CommandShell::mount_cmd(std::string& params, OutputStream& os)
{
    if(mounted) {
        os.printf("Already mounted\n");
        return true;
    }

    int ret;
    ret = boardctl(BOARDIOC_INIT, 0);
    if(OK != ret) {
        os.printf("Failed to INIT SDIO\n");
        return true;
    }

    const char g_target[]         = "/sd";
    const char g_filesystemtype[] = "vfat";
    const char g_source[]         = "/dev/mmcsd0";

    ret = mount(g_source, g_target, g_filesystemtype, 0, nullptr);
    if(0 != ret) {
        os.printf("Failed to mount sdcard\n");
        return true;
    }

    mounted = true;
    os.printf("Mounted %s on %s\n", g_source, g_target);
    return true;
}

bool CommandShell::cat_cmd(std::string& params, OutputStream& os)
{
    // Get parameters ( filename and line limit )
    std::string filename          = shift_parameter( params );
    std::string limit_parameter   = shift_parameter( params );
    int limit = -1;

    if ( limit_parameter != "" ) {
        char *e = NULL;
        limit = strtol(limit_parameter.c_str(), &e, 10);
        if (e <= limit_parameter.c_str())
            limit = -1;
    }

    // Open file
    FILE *lp = fopen(filename.c_str(), "r");
    if (lp == NULL) {
        os.printf("File not found: %s\n", filename.c_str());
        return true;
    }
    char buffer[132];
    int newlines = 0;
    // Print each line of the file
    while (fgets (buffer, sizeof(buffer)-1, lp) != nullptr) {
        os.printf(buffer);
        usleep(1000000);
        if ( limit > 0 && ++newlines >= limit ) {
            break;
        }
    };
    fclose(lp);

    return true;
}
