class evictionCache{
    int capacity;
    list<pair<int,int>> dll;
    unordered_map<int,list<pair<int,int>>::iterator> mp;

    void moveToFront(int key){
        auto it = mp[key];
        dll.splice(dll.begin(),dll,it);
    }
public:
    evictionCache(int cap){
        this->capacity = cap;
    }
    int get(int key){
        if(mp.find(key) == mp.end())
            return -1;
        moveToFront(key);
        return mp[key]->second;
    }

    void set(int key, int val){
        if(mp.find(key) != mp.end()){
            mp[key]->second = val;
            moveToFront(key);
            return; 
        }
        
        if(dll.size() == capacity){
            auto temp = dll.back();
            mp.erase(temp.first);
            dll.pop_back();
        }

        dll.emplace_front(key,val);
        mp[key] = dll.begin();
    }

    void display(){
        for(auto& en : dll)
            cout << en.first << ':' << en.second << ',';
        cout << '\n';
    }
};