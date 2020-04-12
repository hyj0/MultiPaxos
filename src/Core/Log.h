//
// Created by dell-pc on 2018/5/6.
//

#ifndef PROJECT_LOG_H
#define PROJECT_LOG_H
#include <cstring>
#include <iostream>
using namespace std;
#define LVAR(var)   " " << #var << "="<< var
#define LOG_COUT cout << __FILE__ << ":" << __LINE__ <<  " at " << __FUNCTION__ << " "
#define LOG_ENDL_ERR      LVAR(errno) << " err=" << strerror(errno) << endl
#define LOG_ENDL " " << endl;
class Log {

};


#endif //PROJECT_LOG_H
