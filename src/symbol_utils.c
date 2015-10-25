/* 
 * 本软件为免费、开源软件。
 * 本软件的版权(包括源码及二进制发布版本)归一切公众所有。
 * 您可以自由使用、传播本软件。
 * 您也可以以任何形式、任何目的使用本软件(包括源码及二进制发布版本)，而不受任何版权限制。
 * =====================
 * 作者: 孙明保
 * 邮箱: sunmingbao@126.com
 */

#include <stdint.h>
#include <inttypes.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <errno.h>
#include <dlfcn.h>
#include <pthread.h>
#include <termios.h>
#include "debug.h"
#include "string_utils.h"

static  void *handle;

#define   MAX_CMD_LEN    (256)
#define   MAX_ARG_NUM    (8)


typedef  void (*func_type)(long para1, ...);

static int parse_function_args(long *args, char *para_str_begin, char *para_str_end)

{
    int arg_nr = 0;
    char *p_tmp;
    char *cur_arg = para_str_begin;

    if (para_str_begin==para_str_end) return 0;

    p_tmp = para_str_begin-1;
    do 
    {
        cur_arg = p_tmp+1;
        if (cur_arg==para_str_end) return -1;
        if (!isdigit(cur_arg[0]) && cur_arg[0]!='"') return -1;

        if (isdigit(cur_arg[0]))
        {
            args[arg_nr] = strtol(cur_arg,NULL,0);
        }
        else if (cur_arg[0]=='"')
        {
            cur_arg++;
            args[arg_nr] = (long)(unsigned long)(void *)cur_arg;
            
            cur_arg = strchr(cur_arg, '"');
            if (!cur_arg)
                return -1;
            *cur_arg = 0;
            cur_arg++;
        }
        else
            return -1;
        
        arg_nr++;
        p_tmp = strchr(cur_arg, ',');
    } while (p_tmp && arg_nr<MAX_ARG_NUM);

    return 0;
}


static int parse_func_call(char *buf, func_type *fun_addr, long *args)
{
    char *func_name, *para_str_begin, *para_str_end;
    int ret;

    func_name = buf;
    para_str_begin = strchr(buf, '(');
    *para_str_begin = 0;
    para_str_begin++;
    
    *fun_addr = dlsym(handle, buf);
    if (!(*fun_addr))
    {
        printf("unknown function %s", func_name);
        return -1;
    }

    para_str_end = strrchr(para_str_begin, ')');
    if (!para_str_end)
    {
        printf("invalid function call syntax");
        return -1;
    }
    *para_str_end = 0;

    ret = parse_function_args(args, para_str_begin, para_str_end);
    if (ret)
    {
        printf("invalid function call syntax");
    }

    return ret;
}


static void do_call_func(func_type pfunc, long *args)
{
    pfunc(args[0]
        ,args[1]
        ,args[2]
        ,args[3]
        ,args[4]
        ,args[5]
        ,args[6]
        ,args[7]);
}


static void show_var_info(const char *var_name)
{
    void *var_addr = dlsym(handle, var_name);
    if (!var_addr)
    {
        printf("unknown symbol %s", var_name);
        return;
    }

    printf("[var address] : %p\n\n"
        "[values] :\n"
        "1 byte :0x%-16"PRIx8  " (%"PRIi8")\n"
        "2 bytes:0x%-16"PRIx16 " (%"PRIi16")\n"
        "4 bytes:0x%-16"PRIx32 " (%"PRIi32")\n"
        "8 bytes:0x%-16"PRIx64 " (%"PRIi64")\n"
        
        , var_addr
        
        , *(uint8_t *)var_addr, *(int8_t *)var_addr
        , *(uint16_t *)var_addr, *(int16_t *)var_addr
        , *(uint32_t *)var_addr, *(int32_t *)var_addr
        , *(uint64_t *)var_addr, *(int64_t *)var_addr);

}

static void assign_var(char *buf)
{
    char *var_name, *para_str_begin;
    void *var_addr;

    var_name = buf;
    para_str_begin = strchr(buf, '=');
    *para_str_begin = 0;
    para_str_begin++;
    
    var_addr = dlsym(handle, var_name);
    if (!var_addr)
    {
        printf("unknown var %s", var_name);
        return;
    }

    if (!isdigit(para_str_begin[0]))
    {
        printf("invalid variable assign syntax");
        return;
    }

    *(uint64_t *)var_addr=strtol(para_str_begin,NULL,0);
    show_var_info(var_name);

}

static void exec_function(char *call_func_str)
{
    func_type pfunc;
    long  args[MAX_ARG_NUM];
    
    int ret = parse_func_call(call_func_str, &pfunc, args);


    if (!ret)
        do_call_func(pfunc, args);


}


void d(void *start_addr, long length)
{
    char str_addr[32];
    char str_data[64];
    char str_readable[32];
    unsigned char *cur_pos = start_addr;
    int i;


    while (length >= 16)
    {
        sprintf(str_addr, "%-16lx", (unsigned long)(void *)cur_pos);
        for (i = 0; i < 16; i++)
        {
            sprintf(str_data + i*3, "%02hhx ", cur_pos[i]);
            if (cur_pos[i] > 31 &&  cur_pos[i] < 127)
            sprintf(str_readable + i, "%c", (char)(cur_pos[i]));
            else
            sprintf(str_readable + i, "%c", '*');
        }
        length -= 16;
        cur_pos += 16;
        printf("%s: %s: %s\n", str_addr, str_data, str_readable);
    }


    if (length > 0)
    {
        sprintf(str_addr, "%-16lx", (unsigned long)(void *)cur_pos);
        for (i = 0; i < length; i++)
        {
            sprintf(str_data + i*3, "%02hhx ", cur_pos[i]);
            if (cur_pos[i] > 31 &&  cur_pos[i] < 127)
            sprintf(str_readable + i, "%c", (char)(cur_pos[i]));
            else
            sprintf(str_readable + i, "%c", '*');
        }
        for (i = length; i < 16; i++)
        {
            sprintf(str_data + i*3, "%s", "   ");
            sprintf(str_readable + i, "%c", ' ');
        }
        printf("%s: %s: %s\n", str_addr, str_data, str_readable);
    }

}


void proccess_cmd(char *cmd_line)
{
    if (strchr(cmd_line, '(')==NULL)
    {
        if (strchr(cmd_line, '=')==NULL)
            show_var_info(cmd_line);
        else
            assign_var(cmd_line);
    }
    else
        exec_function(cmd_line);

    fflush(stdout);

}


int __attribute__((constructor, used)) init_symbol()
{
    handle = dlopen(NULL, RTLD_NOW);
    if (!handle) 
    {
        DBG_PRINT("%s\n", dlerror());
        exit(1);
    }

    return 0;
}
