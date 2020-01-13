#include <iostream>
#include <yaml-cpp/yaml.h>

using namespace std;

template <typename T>
void operator>>(const YAML::Node& node, T& i)
{
    i = node.as<T>();
};

int main()
{
    // 1.Load yaml file
    YAML::Node node = YAML::LoadFile("./conf/nutcracker.yml");

    cout << node["alpha"] << endl;
    for(auto n=node.begin(); n!=node.end(); n++)
    {
        cout << n->first.as<std::string>() << endl;
        auto second = n->second;
        if (second["hash"])
        {
            cout << "find hash value:"<< second["listen"] << endl;
        }   
        else
        {
            cout << "cannot find hash value" << endl;
        }
        
        if (second["test"])
        {   
            cout << "find test value:"<< second["test"] << endl;
        }
        else
        {
            cout << "cannot find test value" << endl;
        }

        if (second["servers"])
        {
            //cout << "find servers:" << second["servers"] << endl;
            auto servers = second["servers"];
            cout << "servers num:" << servers.size() << endl;
            for(int i=0; i<servers.size(); i++)
            {   
                cout << servers[i] << endl;
            }
        }
        else
        {   
            cout << "cannot find servers value" << endl;
        }
    }
    return 0;
}

