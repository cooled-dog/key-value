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


void funcTIMELEFT(char buffer[], int fd){
    char key[100];
    sscanf(buffer, "%*s %s", key);
    bool found;
    {
        lock_guard<mutex> lock(mtx);
        auto exp = expiry.find(key);
        found = (exp != expiry.end());
        if(found){
            if(exp->second <= chrono::steady_clock::now()) 
                found = false;
        }
    }
    if(found){
        string str;
        {
            lock_guard<mutex> lock(mtx);
            auto timestamp = expiry[key] - chrono::steady_clock::now();
            auto timeleft = chrono::duration_cast<chrono::seconds>(timestamp).count();
            str= to_string(timeleft);
        }
        send(fd, str.c_str(), str.size(), 0); 
    }
    else send(fd, "NULL\n", 5, 0); ????????
}