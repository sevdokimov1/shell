#include <iostream>
#include <string>
#include <fstream>
#include <unistd.h>
#include <sys/wait.h>
#include <vector>
#include <sstream>
#include <signal.h>
#include <cstdint>
#include "vfs.hpp"

void sighup_handler(int signal_nubmer)
{
    if (signal_nubmer == SIGHUP)
    {
        std::cout << "Configuration reloaded\n";
        std::cout << "$ ";
    }
}

void check_disk_partitions(const std::string& device_path)
{
    std::ifstream device(device_path, std::ios::binary);
    if (!device)
    {
        std::cout << "Error: Cannot open device " << device_path << "\n";
        return;
    }

    char sector[512];
    device.read(sector, 512);

    if (device.gcount() != 512)
    {
        std::cout << "Error: Cannot read disk\n";
        return;
    }

    if ((unsigned char)sector[510] != 0x55 || (unsigned char)sector[511] != 0xAA)
    {
        std::cout << "Error: Invalid disk signature\n";
        return;
    }

    bool is_gpt = false;
    for (int i = 0; i < 4; i++)
    {
        if ((unsigned char)sector[446 + i * 16 + 4] == 0xEE)
        {
            is_gpt = true;
            break;
        }
    }

    if (!is_gpt)
    {
        for (int i = 0; i < 4; i++)
        {
            int offset = 446 + i * 16;
            unsigned char type = sector[offset + 4];

            if (type != 0) {
                uint32_t num_sectors = *(uint32_t*)&sector[offset + 12];
                uint32_t size_mb = num_sectors / 2048;
                bool bootable = ((unsigned char)sector[offset] == 0x80);

                std::cout << "Partition " << (i + 1) << ": Size=" << size_mb << "MB, Bootable: ";
                if (bootable)
                    std::cout << "Yes\n";
                else std::cout << "No\n";
            }
        }
    }
    else
    {
        device.read(sector, 512);
        if (device.gcount() == 512 && sector[0] == 'E' && sector[1] == 'F' && sector[2] == 'I' && sector[3] == ' ' && sector[4] == 'P' && sector[5] == 'A' &&
            sector[6] == 'R' && sector[7] == 'T')
        {
            uint32_t num_partitions = *(uint32_t*)&sector[80];
            std::cout << "GPT partitions: " << num_partitions << "\n";
        }
        else
        {
            std::cout << "GPT partitions: unknown\n";
        }
    }
}

int main()
{
    std::cout << std::unitbuf;
    std::cerr << std::unitbuf;

    fuse_start();
    std::cerr << "$ ";

    const char* home = std::getenv("HOME");
    std::string historyPath = std::string(home) + "/.kubsh_history";
    std::string input;

    signal(SIGHUP, sighup_handler);

    while (std::getline(std::cin, input))
    {
        if (!input.empty())
        {
            std::ofstream history(historyPath, std::ios::app);
            history << input << "\n";
        }

        if (input == "history")
        {
            std::ifstream historyOutput(historyPath);
            std::string line;
            while (std::getline(historyOutput, line))
            {
                std::cout << line << "\n";
            }
        }
        else if (input == "\\q")
        {
            break;
        }
        else if (input.substr(0, 3) == "\\l ")
        {
            std::string device_path = input.substr(3);
            device_path.erase(0, device_path.find_first_not_of(" \t"));
            device_path.erase(device_path.find_last_not_of(" \t") + 1);

            if (device_path.empty())
            {
                std::cout << "Usage: \\l /dev/device_name (e.g., \\l /dev/sda)\n";
            }
            else
            {
                check_disk_partitions(device_path);
            }
        }
        else if (input.substr(0, 7) == "debug '" && input[input.length() - 1] == '\'')
        {
            std::cout << input.substr(7, input.length() - 8) << std::endl;
            continue;
        }
        else if (input.substr(0, 4) == "\\e $")
        {
            std::string varName = input.substr(4);
            const char* value = std::getenv(varName.c_str());

            if (value != nullptr)
            {
                std::string valueStr = value;
                bool has_colon = false;
                for (char c : valueStr)
                {
                    if (c == ':')
                    {
                        has_colon = true;
                        break;
                    }
                }

                if (has_colon)
                {
                    std::string current_part = "";
                    for (char c : valueStr)
                    {
                        if (c == ':')
                        {
                            std::cout << current_part << "\n";
                            current_part = "";
                        }
                        else
                        {
                            current_part += c;
                        }
                    }
                    std::cout << current_part << "\n";
                }
                else
                {
                    std::cout << valueStr << "\n";
                }
            }
            else
            {
                std::cout << varName << ": не найдено\n";
            }
            continue;
        }
        else
        {
            pid_t pid = fork();

            if (pid == 0)
            {
                std::vector<std::string> tokens;
                std::vector<char*> args;
                std::string token;
                std::istringstream iss(input);

                while (iss >> token)
                {
                    tokens.push_back(token);
                }

                for (auto& t : tokens)
                {
                    args.push_back(const_cast<char*>(t.c_str()));
                }
                args.push_back(nullptr);

                execvp(args[0], args.data());

                std::cout << args[0] << ": command not found\n";
                exit(1);
            }
            else if (pid > 0)
            {
                int status;
                waitpid(pid, &status, 0);
            }
            else
            {
                std::cerr << "Failed to create process\n";
            }
        }
        std::cout << "$ ";
    }
}