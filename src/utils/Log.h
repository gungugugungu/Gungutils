//
// Created by gungu on 9/13/25.
//

#ifndef LOG_H
#define LOG_H

void iprint(std::string text) {
    std::cout << "[" << stm_sec(stm_now()) << "]" << "[info] " << text.c_str() << std::endl;
}

void eprint(std::string text) {
    std::cerr << "[" << stm_sec(stm_now()) << "]" << "[error] " << text.c_str() << std::endl;
}

void wprint(std::string text) {
    std::cout << "[" << stm_sec(stm_now()) << "]" << "[warning] " << text.c_str() << std::endl;
}

#endif //LOG_H