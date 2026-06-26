void funcSET(char buffer[]){
    char key[100] , value[100];
    sscanf(buffer, "%*s %s %[^\n]", key, value);
    {
        lock_guard<mutex> lock(store_mtx);
        store[key] = value;
    }        
    send(client_fd, "OK\n", 3, 0);
}

void funcEXPIRE(char buffer[]){
    char key[100];
    int value;
    sscanf(buffer, "%*s %s %d", key, value);
    {
        lock_guard<mutex> lock(expiry_mtx);
        expiry[key] = chrono::steady_clock:now() + chrono::seconds(value);
    }
    send(client_fd, "OK\n", 3, 0);
}

void funcGET(char buffer[]){
    char key[100];
    sscanf(buffer,"%*s %s", key);
    auto it = store.find(key);
    if(it.second = store.end()){
        send(client_fd, "NULL\n", 5, 0);
    }
    else if(chrono::steady_clock::now() >= expiry(key)){
        {
            lock_guard<mutex> lock(store_mtx);
            store.erase(key);
        }
        send(client_fd, "NULL\n", 5, 0);   
    }
    else{
        send(client_fd, it.second.c_str(), it.second.size(), 0);
    }
}

void funcDEL(char buffer[]){
    char key[100];
    sscanf(buffer,"%*s %s", key);
    {
        lock_guard<mutex> lock(store_mtx);
        store.erase(key);
    }
    send(client_fd, "OK\n", 3, 0);
}