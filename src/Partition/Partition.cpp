#include "pch.hpp"
#include "Partition.hpp"

Partition::Partition()
{

}
Partition::~Partition()
{
    std::cerr << "Goodbye from Partition!\n";
}
void Partition::Run()
{
    std::cerr << "Hello from Partition!\n";
}