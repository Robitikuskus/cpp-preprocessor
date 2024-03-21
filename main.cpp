#include <cassert>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <regex>
#include <sstream>
#include <string>
#include <vector>

using namespace std;
using filesystem::path;

path operator""_p(const char* data, std::size_t sz) {
    return path(data, data + sz);
}

bool Preprocess(const path&, const path&, const vector<path>&);

bool PrintToOutFile(const path& file_name, const path& out_file,
       const vector<path>& include_directories) {
    bool found = false;
    for (const auto& dir : include_directories) {
        path include_path = dir / file_name;
        if (filesystem::exists(include_path)) {
            found = true;
            if (!Preprocess(include_path, out_file, include_directories)) {
                return false;
            }
            break;
        }
    }
    return found;
}

bool Preprocess(const path& in_file, const path& out_file,
                const vector<path>& include_directories) {
    ifstream ifs(in_file);
    if (!ifs.is_open()) {
        cerr << "Failed to open input file: "s << in_file << endl;
        return false;
    }

    ofstream ofs(out_file, ios::app);
    if (!ofs.is_open()) {
        cerr << "Failed to open output file: "s << out_file << endl;
        return false;
    }

    string line;
    regex quotes(R"/(\s*#\s*include\s*"([^"]*)"\s*)/");
    regex brackets(R"/(\s*#\s*include\s*<([^>]*)>\s*)/");
    size_t line_number = 0;

    while (getline(ifs, line)) {
        ++line_number;
        smatch match;
        bool found = false;
        if (regex_match(line, match, quotes)) {
            path file_name(match[1].str());
            path include_path = in_file.parent_path() / file_name;
            if (filesystem::exists(include_path)) {
                found = true;
                if (!Preprocess(include_path, out_file, include_directories)) {
                    return false;
                }
            } else {
                found = PrintToOutFile(file_name, out_file, include_directories);
            }

            if (!found) {
                cout << "unknown include file "s << file_name.string() 
                    << " at file "s << in_file.string()
                    << " at line "s << line_number << endl;
                return false;
            }

        } else if (regex_match(line, match, brackets)) {
            path file_name(match[1].str());
            found = PrintToOutFile(file_name, out_file, include_directories);

            if (!found) {
                cout << "unknown include file "s << file_name.string()
                    << " at file "s << in_file.string()
                    << " at line "s << line_number << endl;
                return false;
            }

        } else {
            ofs << line << endl;
        }
    }
    return true;
}

string GetFileContents(string file) {
    ifstream stream(file);

    // конструируем string по двум итераторам
    return {(istreambuf_iterator<char>(stream)), istreambuf_iterator<char>()};
}

void Test() {
    error_code err;
    filesystem::remove_all("sources"_p, err);
    filesystem::create_directories("sources"_p / "include2"_p / "lib"_p, err);
    filesystem::create_directories("sources"_p / "include1"_p, err);
    filesystem::create_directories("sources"_p / "dir1"_p / "subdir"_p, err);

    {
        ofstream file("sources/a.cpp");
        file << "// this comment before include\n"
                "#include \"dir1/b.h\"\n"
                "// text between b.h and c.h\n"
                "#include \"dir1/d.h\"\n"
                "\n"
                "int SayHello() {\n"
                "    cout << \"hello, world!\" << endl;\n"
                "#   include<dummy.txt>\n"
                "}\n"s;
    }
    {
        ofstream file("sources/dir1/b.h");
        file << "// text from b.h before include\n"
                "#include \"subdir/c.h\"\n"
                "// text from b.h after include"s;
    }
    {
        ofstream file("sources/dir1/subdir/c.h");
        file << "// text from c.h before include\n"
                "#include <std1.h>\n"
                "// text from c.h after include\n"s;
    }
    {
        ofstream file("sources/dir1/d.h");
        file << "// text from d.h before include\n"
                "#include \"lib/std2.h\"\n"
                "// text from d.h after include\n"s;
    }
    {
        ofstream file("sources/include1/std1.h");
        file << "// std1\n"s;
    }
    {
        ofstream file("sources/include2/lib/std2.h");
        file << "// std2\n"s;
    }

    assert((!Preprocess("sources"_p / "a.cpp"_p, "sources"_p / "a.in"_p,
                                  {"sources"_p / "include1"_p,"sources"_p / "include2"_p})));

    ostringstream test_out;
    test_out << "// this comment before include\n"
                "// text from b.h before include\n"
                "// text from c.h before include\n"
                "// std1\n"
                "// text from c.h after include\n"
                "// text from b.h after include\n"
                "// text between b.h and c.h\n"
                "// text from d.h before include\n"
                "// std2\n"
                "// text from d.h after include\n"
                "\n"
                "int SayHello() {\n"
                "    cout << \"hello, world!\" << endl;\n"s;

    assert(GetFileContents("sources/a.in"s) == test_out.str());
}

int main() {
    Test();
}