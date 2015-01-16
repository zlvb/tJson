#include "tjson.h"
#include <stdio.h>
#include <iostream>
#include <string>
#include <string.h>
#ifdef _MSC_VER
#include <windows.h>
#else
#include <sys/time.h>
#endif
void dump_print(const tjson::Value &root, std::string &inden, bool bInden);
#define FUNC_TEST 0
#ifdef _MSC_VER
#define TESTFILE 1
#else
#define TESTFILE 1
#endif
int main(int argc, char *argv[])
{    
    tjson::Value root;
#if TESTFILE
    FILE *fd = fopen(argv[1], "r");
    fseek(fd, 0, SEEK_END);
    size_t len = ftell(fd);
    fseek(fd, 0, SEEK_SET);
    char *buff = (char*)malloc(len+1);
    fread(buff, len, 1, fd);
    fclose(fd);
    buff[len] = 0;
    int r = 0;
#if FUNC_TEST
    r = (int)tjson::parse(buff, len, &root);
#else
    //tjson::Value _root[100];
#ifdef _MSC_VER
    DWORD start = GetTickCount();
#else
    timeval start;
    timeval end;    
    gettimeofday(&start, NULL);
#endif    

    for (int i = 0; i < 100; i++)
    {         
        tjson::Value _root;
        r = (int)tjson::parse(buff, len, &_root);
    }
#ifdef _MSC_VER
    DWORD diff = GetTickCount() - start;
#else
    gettimeofday(&end, NULL);
    unsigned long diff = (1000000 * (end.tv_sec - start.tv_sec)+end.tv_usec-start.tv_usec)/1000;
#endif
#endif
    free(buff);
    size_t slen = len;
    const char *source = buff;
#else    
    std::string str;
    std::getline(std::cin, str);
    int r = (int)tjson::parse(str.c_str(), str.size(), &root);
    size_t slen = str.size();
    const char *source = str.c_str();
#endif
        
    if (r != 0)
    {
        char errorstr[20] = {0};
        char errptr[20] = {0};        
        r--;
        int end = (int)r + 3;
        int start = (int)r - 17;
        if (end > (int)slen)
        {
            end = (int)slen;
        }
        if (start < 0)
        {
            start = 0;
        }
        if (r > end)
        {
            r = end;
        }

        strncpy(errorstr, source+start, end - start);
        memset(errptr, ' ', r-start);
        errptr[r-start] = '^';

        printf("\nerror:\n%s\n%s\n", errorstr, errptr);
    }
    else
    {
        std::string inden;
#if FUNC_TEST
        //dump_print(root, inden, false);
#else
        printf("time:%d\n", (int)diff);
#endif
        printf("\n%s\n", "parse ok");        
    }

#ifdef _MSC_VER
    system("pause");
#endif
    return 0;
}

void dump_print( const tjson::Value &root, std::string &inden, bool bInden )
{    
    if (root.IsString())
    {
        printf("%s\"%s\"", bInden?inden.c_str():"", root.AsString());
    }
    else if (root.IsInteger())
    {
        printf("%s%lld", bInden?inden.c_str():"", root.AsInteger());
    }
    else if (root.IsFloat())
    {
        printf("%s%lf", bInden?inden.c_str():"", root.AsFloat());
    }
    else if (root.IsBool())
    {
        printf("%s%s", bInden?inden.c_str():"", root.AsBool()?"true":"false");
    }
    else if (root.IsNull())
    {
        printf("%s%s", bInden?inden.c_str():"", "null");
    }
    else if (root.IsArray())
    {
        printf("%s%s\n", bInden?inden.c_str():"", "[");
        inden += "  ";
        for (size_t i = 0; i < root.Size(); i++)
        {
            dump_print(root[i], inden, true);
            if (i != root.Size() - 1)
            {
                printf("%s\n", ",");
            }
        }
        inden.resize(inden.size() - 2);
        printf("\n%s%s", inden.c_str(), "]");
    }
    else if (root.IsObject())
    {        
        printf("%s%s\n", bInden?inden.c_str():"", "{");
        inden += "  ";
        std::vector<const char*> keys;
        root.GetKeys(&keys);
        for (size_t i = 0; i < keys.size(); i++)
        {
            printf("%s\"%s\":", inden.c_str(), keys[i]);
            dump_print(root[keys[i]], inden, false);
            if (i != keys.size() - 1)
            {
                printf("%s\n", ",");
            }           
        }
        inden.resize(inden.size() - 2);
        printf("\n%s%s", inden.c_str(), "}");
    }
}
