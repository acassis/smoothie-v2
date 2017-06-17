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
    bool ls_cmd(std::string& params, OutputStream& os);
    bool rm_cmd(std::string& params, OutputStream& os);
    bool mem_cmd(std::string& params, OutputStream& os);
    bool mount_cmd(std::string& params, OutputStream& os);
    bool cat_cmd(std::string& params, OutputStream& os);

    bool mounted;
};