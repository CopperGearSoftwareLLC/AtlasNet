#include "Partition/Partition.hpp"
#include <iostream>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include "pch.hpp"
int main(void)
{
    Partition::Get();
}
