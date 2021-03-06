#pragma once

#include <string>

class OutputStream;

class CommandShell
{
public:
    CommandShell();
    ~CommandShell(){};

    bool initialize();

private:
    bool help_cmd(std::string& params, OutputStream& os);
    bool ls_cmd(std::string& params, OutputStream& os);
    bool rm_cmd(std::string& params, OutputStream& os);
    bool mem_cmd(std::string& params, OutputStream& os);
    bool mount_cmd(std::string& params, OutputStream& os);
    bool cat_cmd(std::string& params, OutputStream& os);
    bool md5sum_cmd(std::string& params, OutputStream& os);
    bool switch_cmd(std::string& params, OutputStream& os);
    bool modules_cmd(std::string& params, OutputStream& os);
    bool gpio_cmd(std::string& params, OutputStream& os);
    bool get_cmd(std::string& params, OutputStream& os);
    bool grblDP_cmd(std::string& params, OutputStream& os);

    bool mounted;
};
