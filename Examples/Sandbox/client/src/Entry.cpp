

#include "SandboxClient.hpp"
int main(int argc, char **argv)
{
    SandboxClient cl;
    SandboxClient::RunArgs args;
    if (argc >= 2)
    {
        try
        {
            args.GameCoordinatorIP = IPAddress::FromString(argv[1]);
        }
        catch (...)
        {
            cl.GetLogger().ErrorFormatted("Failed to parse IP from argv[1], using 127.0.0.1:{}", _PORT_GAMECOORDINATOR);
            IPAddress ip;
            ip.SetIPv4(127, 0, 0, 1, _PORT_GAMECOORDINATOR);
            args.GameCoordinatorIP = ip;
        }
    }
    else
    {
        IPAddress ip;
        ip.SetIPv4(127, 0, 0, 1, _PORT_GAMECOORDINATOR);
        args.GameCoordinatorIP = ip;
    }
    cl.GetLogger().DebugFormatted("using gamecoordinator ip {}", args.GameCoordinatorIP.ToString());

    cl.Run(args);
    return 0;
}