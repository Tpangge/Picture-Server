#pragma once

#include <mysql/mysql.h>
#include <cstdio>
#include <cstdlib>
#include <jsoncpp/json/json.h>
#include<string>

namespace image_system
{
    static MYSQL *MySQLInit()
    {
        //使用mysql API来操作数据库
        //1、先创建一个mysql的句柄
        MYSQL* mysql = mysql_init(NULL);
        //2、使用句柄和数据库建立连接
        if (mysql_real_connect(mysql,"127.0.0.1","root","root","image_system",3306,NULL,0) == NULL)
        {
            //数据库连接失败
            printf("连接失败%s\n",mysql_error(mysql));
            return NULL;
        }
        //3、设置编码格式
        mysql_set_character_set(mysql,"utf8");
        return mysql;    
    }        

    static void MySQLRelease(MYSQL* mysql)
    {
        mysql_close(mysql);
    }
    
    //操作数据库中的image_table表；
    //由于insert等操作，函数依赖的输入信息较多
    //为了防止参数过多，使用json来封装参数

    class ImageTable
    {
    public:
        ImageTable(MYSQL* mysql):_mysql(mysql) {}
        //image就形如下面的形式：
        //{
        //  image_name:"test.png",
        //  size:1024,
        //  upload_time:"2019/09/02",
        //  type: "png",
        //  path: "data/test.png",
        //  md5: "abcd"
        //}
        //使用Json的原因：
        //1、扩展方便；
        //2、方便和服务器接收到的数据相通
        
        bool Insert(const Json::Value& image) 
        {
            char sql[4 * 1024] = {0};
            sprintf(sql,"insert into image_table values(null,'%s',%d,'%s','%s','%s','%s')",
            image["image_name"].asCString(),
            image["size"].asInt(),image["upload_time"].asCString(),
            image["type"].asCString(),image["path"].asCString(),
            image["md5"].asCString());
            printf ("[Insert sql] %s\n",sql);
            
            int ret = mysql_query(_mysql,sql);
            if(ret != 0)
            {
                printf ("Insert 执行SQL 失败！ %s\n",mysql_error(_mysql));
                return false;
            }
            return true;

        }

        bool SelectALL(Json::Value* images) 
        {
            char sql[1024 * 4] = {0};
            sprintf(sql,"select * from image_table");
            int ret = mysql_query(_mysql,sql);
            if(ret != 0)
            {
                printf("SelectALL 执行 SQL 失败！%s\n",mysql_error(_mysql));
                return false;   
            }
            //遍历结果集合，并把结果集写到image 参数中
            MYSQL_RES* result = mysql_store_result(_mysql);
            int rows = mysql_num_rows(result);
            for(int i = 0; i < rows; ++i)
            {
                MYSQL_ROW row = mysql_fetch_row(result);
                //数据库查出的每条记录都相当于一个图片信息
                //需要吧这个信息转成JSON格式
                Json::Value image;
                image["image_id"] = atoi(row[0]);
                image["image_name"] = row[1];
                image["size"] = atoi(row[2]);
                image["upload_time"] = row[3];
                image["type"] = row[4];
                image["path"] = row[5];
                image["md5"] = row[6];
                images->append(image);
             }
             //释放结果集合，忘记的话会导致内存泄漏
             mysql_free_result(result);
             return true;
        }
        
        bool SelectOne(int image_id,Json::Value* image_ptr)
        {
            char sql[1024 * 4] = {0};
            sprintf(sql,"select * from image_table where image_id = %d",image_id);
            int ret = mysql_query(_mysql,sql);
            if(ret != 0)
            {
                printf("SelectOne 执行 SQL 失败！%s\n",mysql_error(_mysql));
                return false;         
            }

            MYSQL_RES* result = mysql_store_result(_mysql);
            int rows = mysql_num_rows(result);
            if(rows != 1)
            {
                printf("SelectOne 查询结果不是1条记录! 实际查询到%d条！\n",rows);
             return false;       
            }
            MYSQL_ROW row = mysql_fetch_row(result);
            Json::Value image;
            image["image_id"] = atoi(row[0]);
            image["image_name"] = row[1];
            image["size"] = atoi(row[2]);
            image["upload_time"] = row[3];
            image["type"] = row[4];
            image["path"] = row[5];
            image["md5"] = row[6];
            *image_ptr = image;
            //释放结果集合
            mysql_free_result(result);
            return true;
        }

        bool Delete(int image_id)
        {
            char sql[4 * 1024] = {0};
            sprintf(sql,"delete from image_table where image_id = %d",image_id);
            int ret = mysql_query(_mysql,sql);
            if(ret != 0)
            {
                printf("Delete 执行 SQL 失败！%s\n",mysql_error(_mysql));
                return false; 
            }
            return true;
        }
        
    private:
        MYSQL*  _mysql;
    
    };
} // image_system 命名空间结束
