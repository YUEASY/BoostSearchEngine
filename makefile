PARSER=parser
DEBUG=debug
HTTP=http_server
cc=g++

.PHONY:all
all:$(PARSER) $(DEBUG) $(HTTP)

$(PARSER):parser.cc
	$(cc) -o $@ $^ -L/usr/lib64/mysql -lmysqlclient -ljsoncpp -lboost_system -lboost_filesystem -std=c++11
$(DEBUG):debug.cc
	$(cc) -o $@ $^ -L/usr/lib64/mysql -lmysqlclient -ljsoncpp -lpthread -std=c++11
$(HTTP):http_server.cc
	$(cc) -o $@ $^ md5.cpp -L/usr/lib64/mysql  -lmysqlclient -ljsoncpp -lpthread -std=c++11
.PHONY:clean
clean:
	rm -f $(PARSER) $(DEBUG) $(HTTP)