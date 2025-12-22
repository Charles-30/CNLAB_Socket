### Compile
```bash
g++ -o client client.cpp -lglog -lpthread
g++ -o server server.cpp -lglog -lpthread
g++ -o client_test client_test.cpp -lglog -lpthread
```

### Execute
```bash
./server
./client
```
You must start server before you start any client
