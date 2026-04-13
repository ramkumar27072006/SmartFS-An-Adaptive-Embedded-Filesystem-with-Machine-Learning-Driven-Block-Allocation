#include "smartfs.h"
#include "fsck.h"
#include "viewer.h"
#include <iostream>
#include <string>
#include <sstream>
#include <cstring>

// ============================================================
// shell.exe — Interactive filesystem shell
// Commands: format, mount, unmount, create, write, read,
//           delete, ls, gc, fsck, map, wear, mode, help, exit
// ============================================================

static const char *DEFAULT_IMAGE = "sd.img";

void printHelp()
{
    std::cout << "\nSmartFS Shell Commands:\n"
              << "  format [image]        Format filesystem\n"
              << "  mount  [image]        Mount filesystem\n"
              << "  unmount               Unmount filesystem\n"
              << "  create <name>         Create empty file\n"
              << "  write  <name> <text>  Write text to file\n"
              << "  read   <name>         Read file contents\n"
              << "  delete <name>         Delete file\n"
              << "  ls                    List files\n"
              << "  gc                    Run garbage collector\n"
              << "  fsck                  Run consistency check\n"
              << "  map                   Show block map\n"
              << "  wear                  Show wear statistics\n"
              << "  mode                  Show current alloc mode\n"
              << "  help                  Show this help\n"
              << "  exit                  Exit shell\n\n";
}

int main()
{
    SmartFS fs;
    bool mounted = false;
    std::string currentImage = DEFAULT_IMAGE;

    std::cout << "=== SmartFS Interactive Shell ===\n";
    printHelp();

    std::string line;
    while (true)
    {
        std::cout << "smartfs> ";
        if (!std::getline(std::cin, line))
            break;

        std::istringstream iss(line);
        std::string cmd;
        iss >> cmd;

        if (cmd.empty())
            continue;

        if (cmd == "exit" || cmd == "quit")
        {
            if (mounted)
                fs.unmount();
            break;
        }
        else if (cmd == "help")
        {
            printHelp();
        }
        else if (cmd == "format")
        {
            if (mounted)
            {
                fs.unmount();
                mounted = false;
            }
            std::string img;
            iss >> img;
            if (img.empty())
                img = currentImage;
            else
                currentImage = img;

            fs.format(img);
        }
        else if (cmd == "mount")
        {
            if (mounted)
            {
                std::cout << "Already mounted. Unmount first.\n";
                continue;
            }
            std::string img;
            iss >> img;
            if (img.empty())
                img = currentImage;
            else
                currentImage = img;

            if (fs.mount(img))
                mounted = true;
        }
        else if (cmd == "unmount")
        {
            if (!mounted)
            {
                std::cout << "Not mounted.\n";
                continue;
            }
            fs.unmount();
            mounted = false;
        }
        else if (cmd == "create")
        {
            if (!mounted) { std::cout << "Not mounted.\n"; continue; }
            std::string name;
            iss >> name;
            if (name.empty()) { std::cout << "Usage: create <name>\n"; continue; }
            fs.create(name.c_str());
        }
        else if (cmd == "write")
        {
            if (!mounted) { std::cout << "Not mounted.\n"; continue; }
            std::string name;
            iss >> name;
            if (name.empty()) { std::cout << "Usage: write <name> <text>\n"; continue; }

            std::string text;
            std::getline(iss, text);
            // Trim leading space
            if (!text.empty() && text[0] == ' ')
                text = text.substr(1);
            if (text.empty()) { std::cout << "Usage: write <name> <text>\n"; continue; }

            fs.write(name.c_str(), text.c_str(), static_cast<int>(text.size()));
        }
        else if (cmd == "read")
        {
            if (!mounted) { std::cout << "Not mounted.\n"; continue; }
            std::string name;
            iss >> name;
            if (name.empty()) { std::cout << "Usage: read <name>\n"; continue; }

            char buf[8192];
            int bytesRead = 0;
            if (fs.read(name.c_str(), buf, 8192, bytesRead))
            {
                std::cout << "Content (" << bytesRead << " bytes):\n";
                std::cout.write(buf, bytesRead);
                std::cout << "\n";
            }
        }
        else if (cmd == "delete" || cmd == "rm")
        {
            if (!mounted) { std::cout << "Not mounted.\n"; continue; }
            std::string name;
            iss >> name;
            if (name.empty()) { std::cout << "Usage: delete <name>\n"; continue; }
            fs.del(name.c_str());
        }
        else if (cmd == "ls" || cmd == "list")
        {
            if (!mounted) { std::cout << "Not mounted.\n"; continue; }
            fs.listFiles();
        }
        else if (cmd == "gc")
        {
            if (!mounted) { std::cout << "Not mounted.\n"; continue; }
            fs.gc();
        }
        else if (cmd == "fsck")
        {
            if (!mounted) { std::cout << "Not mounted.\n"; continue; }
            Fsck checker;
            checker.init(&fs.getBlockDevice());
            checker.check();
        }
        else if (cmd == "map")
        {
            if (!mounted) { std::cout << "Not mounted.\n"; continue; }
            Viewer viewer;
            viewer.init(&fs.getBlockDevice());
            viewer.printMap();
        }
        else if (cmd == "wear")
        {
            if (!mounted) { std::cout << "Not mounted.\n"; continue; }
            fs.getWearTracker().printStats();
        }
        else if (cmd == "mode")
        {
            if (!mounted) { std::cout << "Not mounted.\n"; continue; }
            int mode = fs.getAllocMode();
            const char *names[] = {"SEQUENTIAL", "RANDOM", "WEAR-AWARE"};
            if (mode >= 0 && mode <= 2)
                std::cout << "Current allocation mode: " << names[mode] << "\n";
        }
        else
        {
            std::cout << "Unknown command: " << cmd << ". Type 'help' for commands.\n";
        }
    }

    std::cout << "Goodbye.\n";
    return 0;
}
