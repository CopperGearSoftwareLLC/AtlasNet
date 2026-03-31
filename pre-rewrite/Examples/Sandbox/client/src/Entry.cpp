

#include "SandboxClient.hpp"
int main(int argc, char **argv)
{
	IPAddress address;
	if (argc > 1)
	{
		address.Parse(std::string(argv[1]));
	}
	else
	{
		address = IPAddress::MakeLocalHost(_PORT_PROXY_PUBLISHED);
	}
	SandboxClient cl;
	cl.Run(address);
	return 0;
}