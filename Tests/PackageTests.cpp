#include "Setup.h"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <objbase.h>
int main(){
    namespace fs=std::filesystem;
    wchar_t temp[MAX_PATH]{};if(!GetTempPathW(MAX_PATH,temp))return 1;GUID id{};if(FAILED(CoCreateGuid(&id)))return 1;wchar_t text[40]{};StringFromGUID2(id,text,40);
    fs::path file=fs::path(temp)/(std::wstring(L"ProcessTracePolicy-")+text+L".inf");
    struct Remove{fs::path path;~Remove(){DeleteFileW(path.c_str());}} cleanup{file};
    unsigned checks=0;auto require=[&](bool value){++checks;if(!value){std::cerr<<"Package policy check failed: "<<checks<<'\n';std::exit(1);}};
    auto write=[&](const std::string& hardware,const std::string& provider,const std::string& catalog){
        std::ofstream out(file,std::ios::binary|std::ios::trunc);
        out<<"[Version]\r\nSignature=\"$Windows NT$\"\r\nClass=System\r\nClassGUID={4D36E97D-E325-11CE-BFC1-08002BE10318}\r\nProvider=%Provider%\r\nCatalogFile="<<catalog<<"\r\n[Manufacturer]\r\n%Provider%=Models,NTamd64.10.0...19041\r\n[Models.NTamd64.10.0...19041]\r\nDevice=Install,"<<hardware<<"\r\n[Install]\r\n[Strings]\r\nProvider=\""<<provider<<"\"\r\n";out.close();require(!out.fail());
    };
    write("ROOT\\TitanProcessTrace","Titanfyre","TitanProcessTrace.cat");require(Setup::PackageMatches(file.wstring()));require(!Setup::CatalogTrusted(file.wstring()));
    write("ROOT\\DifferentProcessTrace","Titanfyre","TitanProcessTrace.cat");require(!Setup::PackageMatches(file.wstring()));
    write("ROOT\\TitanProcessTrace","AnotherPublisher","TitanProcessTrace.cat");require(!Setup::PackageMatches(file.wstring()));
    write("ROOT\\TitanProcessTrace","Titanfyre","AnotherProcessTrace.cat");require(!Setup::PackageMatches(file.wstring()));
    require(!Setup::PackageMatches((file.wstring()+L".absent")));require(!Setup::CatalogTrusted(file.wstring()+L".absent"));
    std::cout<<checks<<" read-only package identity/trust checks passed; no device installation.\n";return 0;
}
