//
// Created by gungu on 9/13/25.
//

#ifndef LOG_H
#define LOG_H

enum LogType {
    INFO,
    ERROR,
    WARNING,
};

struct Log {
    std::string text;
    LogType type = INFO;
};

const int MAX_LOGS = 20;
inline Log logs[MAX_LOGS];

void _add_log(std::string text, LogType type) {
    for (int i = 1; i < MAX_LOGS; i++) {
        logs[i - 1] = logs[i];
        logs[MAX_LOGS-1] = {text, type};
    }
};

inline void iprint(std::string text) {
    std::string output_text = "[" + std::to_string(stm_sec(stm_now())) + "] [INFO] " + text;
    std::cout << output_text.c_str() << std::endl;
    _add_log(output_text, INFO);
}

inline void eprint(std::string text) {
    std::string output_text = "[" + std::to_string(stm_sec(stm_now())) + "] [ERROR] " + text;
    std::cerr << output_text.c_str() << std::endl;
    _add_log(output_text, ERROR);
}

inline void wprint(std::string text) {
    std::string output_text = "[" + std::to_string(stm_sec(stm_now())) + "] [WARN] " + text;
    std::cout << output_text.c_str() << std::endl;
    _add_log(output_text, WARNING);
}

#endif //LOG_H