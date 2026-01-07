
//#include "Debug/Crash/CrashHandler.hpp"
int ENTRY_POINT(int argc, char** argv);

int main(int argc, char** argv) {
    //CrashHandler::Get().Init();
    ENTRY_POINT(argc,argv);}
