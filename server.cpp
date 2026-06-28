#include<iostream>
#include<fstream>
#include<cstdlib>
#include<unistd.h>
#include<arpa/inet.h>
#include<sys/socket.h>
#include<netinet/in.h>
#include<string>
#include<vector>
#include<cstring>
#include<unordered_map>
#include<thread>
#include<mutex>
#include<semaphore.h>
#include<chrono>
#include<list>

using namespace std;
static const int NUM_SHARDS = 10;

int whichShard(char key[]){
    return hash<string>{}(key) % NUM_SHARDS; 
}

struct Shard{
    mutex mtx;
    unordered_map<string,chrono::steady_clock::time_point> expiry;
    unordered_map<string,pair<string,list<string>::iterator>> store;
    list<string> dll;
};

class LRUcache{
    private:
        int capacity;
        vector<Shard> shards;
        void evictOne(Shard& shard){
            if(shard.store.size() > capacity){
                string temp = shard.dll.back();
                shard.dll.pop_back();
                shard.store.erase(temp);
                shard.expiry.erase(temp);
            }
        }
    public:
        LRUcache(int cap) : capacity(cap) , shards(NUM_SHARDS){}
        void funcSET(char key[] , char value[]);
        string funcGET(char key[]);
        void funcDEL(char key[]);
        bool funcEXPIRE(char key[], int value);
};

class KVstore{
    private:
        LRUcache cache;
        
    public:
        KVstore(int cap) : cache(cap){}
        void handleClient(int client_fd);
};

void KVstore::handleClient(int client_fd){
    char buffer[1024];
    while(true){
        memset(buffer, 0 , sizeof(buffer));
        
        int r = read(client_fd,buffer, sizeof(buffer));
        if(r <= 0) break;

        char cmd[10] = {0} , key[100] = {0} , value[100] = {0};
        sscanf(buffer,"%s", cmd);
        ofstream aofFile("logs.aof");
        if(strcmp(cmd,"SET") == 0){
            char key[100] , value[100];
            sscanf(buffer, "%*s %s %[^\n]", key, value);
            cache.funcSET(key,value);
            aofFile << buffer << "\n";
            send(client_fd,"OK\n", 3 , 0);
        }
        else if(strcmp(cmd,"GET") == 0){ 
            char key[100];
            sscanf(buffer,"%*s %s", key);
            string str = cache.funcGET(key);
            send(client_fd, str.c_str(), str.size() , 0);
        }
        else if(strcmp(cmd,"DEL") == 0){
            char key[100];
            sscanf(buffer,"%*s %s", key);
            cache.funcDEL(key);
            send(client_fd, "OK\n", 3, 0);
            aofFile << buffer << "\n";
        }
        else if(strcmp(cmd,"EXPIRE") == 0){
            char key[100];
            int value;
            sscanf(buffer, "%*s %s %d", key, &value);
            bool ok = cache.funcEXPIRE(key, value);
            
            if(ok){
                aofFile << buffer << "\n";    
                send(client_fd, "OK\n", 3, 0);
            }
            else send(client_fd, "No key entry!\n", 14, 0);
        }
        else send(client_fd, "command not found!!\n", 20, 0);
    }
    close(client_fd);
}

void LRUcache::funcSET(char key[] , char value[]){
    Shard& shard = shards[whichShard(key)];
    lock_guard<mutex> lock(shard.mtx);

    auto it = shard.store.find(key);
    if(it != shard.store.end()){
        shard.dll.erase(it->second.second);
        shard.expiry.erase(key);
    }

    shard.dll.push_front(key);
    shard.store[key] = {value, shard.dll.begin()};
    while(shard.store.size() > capacity)
        evictOne(shard);
}

string LRUcache::funcGET(char key[]){
    Shard& shard = shards[whichShard(key)];
    lock_guard<mutex> lock(shard.mtx);
    
    auto it = shard.store.find(key);
    if(it == shard.store.end()){
        return "NULL\n";
    }
    auto exp = shard.expiry.find(key);
    if(exp != shard.expiry.end() && chrono::steady_clock::now() >= exp->second){
        shard.dll.erase(it->second.second);
        shard.store.erase(it);
        shard.expiry.erase(key);
        return "NULL\n";
    }
    shard.dll.erase(it->second.second);
    shard.dll.push_front(key);
    it->second.second = shard.dll.begin();
    return (it->second.first + "\n");
}

void LRUcache::funcDEL(char key[]){
    Shard& shard = shards[whichShard(key)];
    lock_guard<mutex> lock(shard.mtx);

    auto it = shard.store.find(key);
    if(it == shard.store.end()){
        return;
    }
    shard.dll.erase(it->second.second);
    shard.store.erase(it);
    shard.expiry.erase(key);
}

bool LRUcache::funcEXPIRE(char key[], int value){
    Shard& shard = shards[whichShard(key)];
    lock_guard<mutex> lock(shard.mtx);
    auto it = shard.store.find(key);
    if(it == shard.store.end()){
        return false;
    }
    shard.expiry[key] = chrono::steady_clock::now() + chrono::seconds(value);
    return true;
}

int main(int argc , char* argv[]){
    int port;
    if(argc < 2)
        port = 6379;
    else
        port = atoi(argv[1]);
    
    int sock_fd = socket(AF_INET, SOCK_STREAM, 0);
    if(sock_fd < 0){
        perror("socket failed");
        exit(1);
    }
    
    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr.s_addr = INADDR_ANY;
    
    int b = bind(sock_fd,(sockaddr*)&addr, sizeof(addr));
    if(b < 0){
        perror("bind failed");
        exit(1);
    }
    int l = listen(sock_fd, 5);
    if(l < 0){
        perror("listen failed");
        exit(1);
    }
    cout << "Server running on " << port << endl;
    
    KVstore kv(2);
    while(true){
        int client_fd = accept(sock_fd,NULL,NULL);
        if(client_fd < 0) perror("accept failed");
        else printf("Connected\n ");

        thread t(&KVstore::handleClient,&kv,client_fd);
        t.detach();
    }
}