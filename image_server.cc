#include <fstream>
#include <signal.h>
#include "db.hpp"
#include <sys/stat.h>
#include "httplib.h"
#include <time.h>
#include <assert.h>
#include <openssl/md5.h>
#include <string>

class FileUtil 
{
public:
    static bool Write(const std::string& file_name,
            const std::string& content)
    {
        std::ofstream file(file_name.c_str());
        if(!file.is_open())
        {
            return false;
        }
        file.write(content.c_str(), content.length());
        file.close();
        return true;
    }

    static bool Read(const std::string& file_name,std::string*content)
    {
        std::ifstream file(file_name.c_str());
        if(!file.is_open())
        {
            return false;
        }
        struct stat st;
        stat(file_name.c_str(),&st);
        content->resize(st.st_size);
        // 一口气把文件读完
        // 需要先知道文件多长
        // char*缓冲区长度
        // int读取多长
        file.read((char*)content->c_str(),content->size());
        file.close();
        return true;
    }
};

MYSQL* mysql = NULL;
std::string StringMD5(const std::string& str);

int main()
{
    using namespace httplib;

    mysql = image_system::MySQLInit();
    image_system::ImageTable image_table(mysql);
    
    signal(SIGINT, [](int)
    {
        image_system::MySQLRelease(mysql);
        exit(0);
    });
    
    Server server;
    // 客户端请求的/hello 路径的时候，会执行一个特定的函数
    // 指定不同的路径对应到不同的函数上，这个过程叫做“设置路由”
    // 服务器中两个重要的概念：
    // 1、请求(Request)
    // 2、响应(Response)
    // [&image_table]这是lambda的重要特性，捕获变量
    // 本来lambda内部是不能直接访问image_table的。
    // 捕获之后就可以访问了，其中&的含义是相当于按引用捕获
    server.Post("/image",[&image_table](const httplib::Request& req,httplib::Response& resp)
            {
            Json::FastWriter writer;
            Json::Value resp_json;
            printf("上传一个图片！\n");
            // 1、对参数进行检验
            auto ret = req.has_file("upload");
            if(!ret)
            {
                printf("文件上传出错！\n");
                resp.status = 404;
                // 可以使用json格式组织一个返回结果
                resp_json["ok"] = false;
                resp_json["reason"] = "上传文件出错，没有需要的upload";
                resp.set_content(writer.write(resp_json),"application/json");
                return ;
            }
            // 2、根据与文件名获取到文件数据file对象
            const auto& file = req.get_file_value("upload"); 
            // body是图片的内容
            const std::string image_body = req.body.substr(file.offset,file.length);
            // 3、把图片属性信息插入到数据库中
            Json::Value image;
            time_t t = time(0);
            char ch[64];
            strftime(ch,sizeof(ch),"%Y-%m-%d %H:%M:%S",localtime(&t));//年-月-日 时：分：秒
            image["image_name"] = file.filename;
            image["size"] = (int)file.length;
            image["upload_time"] = ch;
            image["type"] = file.content_type;
            image["path"] = "./wwwimage/" + file.filename;
            image["md5"] = StringMD5(image_body);
            ret = image_table.Insert(image);
            if(!ret)
            {
                printf("image_table Insert failed!\n");
                resp_json["ok"] = false;
                resp_json["reason"] = "数据库插入失败!";
                resp.status = 500;
                resp.set_content(writer.write(resp_json),"application/json");
                return;
            }
            // 4、把图片保存到指定的磁盘目录中           
            FileUtil::Write(image["path"].asString(),image_body);
            // 5、构造一个响应数据通知客户端上传成功 
            resp_json["ok"] = true;
            resp.status = 200;
            resp.set_content(writer.write(resp_json),"application/json");
            return;
            });

    server.Get("/image",[&image_table](const Request& req,Response& resp)
            {
            (void) req;//没有实际效果
            printf("获取所有图片信息！\n");
            Json::FastWriter writer;
            Json::Value resp_json;
            //1、调用数据库接口来获取数据
            bool ret = image_table.SelectALL(&resp_json);
            if(!ret)
            {
                printf("查询数据库失败！\n");
                resp_json["ok"] = false;
                resp_json["reason"] = "查询数据库失败!";
                resp.status = 500;
                resp.set_content(writer.write(resp_json),"application/json");
                return;
            }
            //2、构造响应结果返回给客户端
            resp.status = 200;
            resp.set_content(writer.write(resp_json),"application/json");

            });
    //1、正则表达式
    //2、原始字符串(raw string)
    server.Get(R"(/image/(\d+))",[&image_table](const Request& req,Response& resp)
            {
            Json::FastWriter writer;
            Json::Value resp_json;
            // 1、先获取到图片id
            int image_id = std::stoi(req.matches[1]);
            printf("获取id为%d的图片信息!\n",image_id);
            // 2、再根据图片id查询数据库
            bool ret = image_table.SelectOne(image_id,&resp_json);
            if(!ret)
            {
                printf("数据库查询出错！\n");
                resp_json["ok"] = false;
                resp_json["reason"] = "数据库查询出错！";
                resp.status = 500;
                resp.set_content(writer.write(resp_json),"application/json");
                return;
            }
            // 3、把查询结果返回给客户端
            resp_json["ok"] = true;
            resp.set_content(writer.write(resp_json),"application/json");
            return;
            });

    server.Get(R"(/show/(\d+))",[&image_table](const Request& req,Response& resp)
            {
                Json::FastWriter writer;
                Json::Value resp_json;
                // 1、根据图片id去数据库中查到对应目录
                int image_id = std::stoi(req.matches[1]);
                printf("获取id为%d的图片内容！\n",image_id);
                Json::Value image;
                bool ret = image_table.SelectOne(image_id,&image);
                if(!ret)
                {
                    printf("读取数据库失败！\n");
                    resp_json["ok"] = false;
                    resp_json["reason"] = "数据库查询出错";
                    resp.status = 404;
                    resp.set_content(writer.write(resp_json),"application/json");
                    return ;
                }
                // 2、根据目录找到文件内容，读取文件内容
                std::string image_body;
                ret = FileUtil::Read(image["path"].asString(),&image_body); 
                if(!ret)
                {
                    printf("读取图片失败！\n");
                    resp_json["ok"] = false;
                    resp_json["reason"] = "读取图片文件失败";
                    resp.status = 500;
                    resp.set_content(writer.write(resp_json),"application/josn");
                    return;
                }
                // 3、把文件内容构成一个响应
                resp.status = 200;
                // 不同的图片，设置的content type 是不一样的。
                // 如果是png应该设为image/png
                // 如果是jpg应该设为image/jpg
                resp.set_content(image_body,image["type"].asCString());
            
            });
    
    server.Delete(R"(/Delete/(\d+))",[&image_table](const Request& req,Response& resp)
            {
                Json::FastWriter writer;
                Json::Value resp_json;
                // 1、根据图片id去数据库中查到对应目录
                int image_id = std::stoi(req.matches[1]);
                printf("删除id为%d的图片内容！\n",image_id);
                // 2、查找对应文件的路径
                Json::Value image;
                bool ret = image_table.SelectOne(image_id,&image);
                if(!ret)
                {
                    printf("删除图片文件失败！\n");
                    resp_json["ok"] = false;
                    resp_json["reason"] = "删除图片文件出错";
                    resp.status = 500;
                    resp.set_content(writer.write(resp_json),"application/json");
                    return ;
                }
                 
                // 3、调用数据库操作进行删除
                ret = image_table.Delete(image_id);
                if(!ret)
                {
                    printf("删除图片文件失败！\n");
                    resp_json["ok"] = false;
                    resp_json["reason"] = "删除图片文件出错";
                    resp.status = 500;
                    resp.set_content(writer.write(resp_json),"application/json");
                    return ;

                }
                // 4、删除磁盘上面的文件
                // C++标准库中，没有删除文件的方法
                // C++17标准中有
                // 使用操作系统中自带的
                unlink(image["path"].asCString());
                
                // 5、构造响应
                resp_json["ok"] = false;
                resp.status = 200;
                resp.set_content(writer.write(resp_json),"application/json");
            });
    
    server.set_base_dir("./wwwimage");
    server.listen("192.168.10.133",9000);
    return 0;
}

std::string StringMD5(const std::string& str)
{
    const int md5_length=16;
    unsigned char MD5result[md5_length];
    // 使用OpenSSL函数计算MD5
      MD5((const unsigned char*)str.c_str(),str.size(),MD5result);
    // 转换成字符串方便查看
    char out[1024] = {0};
    int off = 0;
    for(int i = 0;i < md5_length; ++i)
    {
       off += sprintf(out + off,"%x",MD5result[i]);
    }
    return std::string(out);
}
