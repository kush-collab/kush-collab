#include <iostream>
#include <fstream>
#include <string>
#include <regex>
#include <vector>
#include <sstream>
#include <cstdio>
#include <sys/stat.h>
#include <unistd.h>

class DmesgParser {
public:
    DmesgParser(const std::string& patternsFile) {
        loadPatternsFromFile(patternsFile);
    }

    void parseLog(const std::string& logFilePath = "", const std::string& dmesgArgs = "") {
        std::string logContent = getLogContent(logFilePath, dmesgArgs);
        parseContent(logContent);
    }

private:
    std::vector<std::regex> patternLibrary;

    void loadPatternsFromFile(const std::string& patternsFile) {
        std::ifstream patternFile(patternsFile);
        if (!patternFile.is_open()) {
            std::cerr << "Error opening patterns file: " << patternsFile << std::endl;
            return;
        }
        readPatterns(patternFile);
        patternFile.close();
    }

    void readPatterns(std::ifstream& patternFile) {
        std::string line;
        while (std::getline(patternFile, line)) {
            if (!line.empty()) {
                compilePattern(line);
            }
        }
    }

    void compilePattern(const std::string& line) {
        try {
            patternLibrary.push_back(std::regex(line, std::regex::icase));
        } catch (const std::regex_error& e) {
            std::cerr << "Regex error in pattern: " << line << "\nError: " << e.what() << std::endl;
        }
    }

    std::string getLogContent(const std::string& logFilePath, const std::string& dmesgArgs) {
        if (!logFilePath.empty()) {
            return readLogFile(logFilePath);
        } else {
            return executeDmesgCommand(dmesgArgs);
        }
    }

    std::string readLogFile(const std::string& logFilePath) {
        std::ifstream logFile(logFilePath);
        if (!logFile.is_open()) {
            std::cerr << "Error opening log file: " << logFilePath << std::endl;
            return "";
        }
        std::stringstream buffer;
        buffer << logFile.rdbuf();
        logFile.close();
        return buffer.str();
    }

    std::string executeDmesgCommand(const std::string& dmesgArgs) {
        std::string command = "dmesg " + dmesgArgs;
        std::array<char, 128> buffer;
        std::string result;
        std::shared_ptr<FILE> pipe(popen(command.c_str(), "r"), pclose);
        if (!pipe) {
            std::cerr << "Error executing dmesg command" << std::endl;
            return "";
        }
        while (fgets(buffer.data(), buffer.size(), pipe.get()) != nullptr) {
            result += buffer.data();
        }
        return result;
    }

    void parseContent(const std::string& content) {
        std::istringstream stream(content);
        std::string line;
        while (std::getline(stream, line)) {
            for (const auto& pattern : patternLibrary) {
                if (std::regex_search(line, pattern)) {
                    std::cout << line << std::endl;
                    break;
                }
            }
        }
    }
};

void printHelp(const std::string& programName) {
    std::cout << "Usage: " << programName << " [options]\n";
    std::cout << "Options:\n";
    std::cout << "  -p <patterns_file>   Specify the patterns file to use (default: patterns.txt)\n";
    std::cout << "  -l <log_file>        Specify a log file to parse instead of running 'dmesg'\n";
    std::cout << "  -a <dmesg_args>      Pass arguments to the 'dmesg' command\n";
    std::cout << "  -h                   Display this help message\n";
}

int main(int argc, char* argv[]) {
    std::string patternFilePath = "patterns.txt";
    std::string logFilePath;
    std::string dmesgArgs;

    int opt;
    while ((opt = getopt(argc, argv, "p:l:a:h")) != -1) {
        switch (opt) {
        case 'p':
            patternFilePath = optarg;
            break;
        case 'l':
            logFilePath = optarg;
            break;
        case 'a':
            dmesgArgs = optarg;
            break;
        case 'h':
            printHelp(argv[0]);
            return 0;
        default:
            std::cerr << "Invalid option. Use -h for help." << std::endl;
            return 1;
        }
    }

    DmesgParser parser(patternFilePath);
    parser.parseLog(logFilePath, dmesgArgs);
    return 0;
}

