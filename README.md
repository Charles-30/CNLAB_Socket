### Compile

You need place client.cpp & server.cpp under the root directory with *.h and create a directory named “log”.

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
You must start server before you start any client.
