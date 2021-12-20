#include <cassert>
#include <numeric>
#include <cstdio>
#include <cstring>
#include <stdexcept>
#include <string>
#include <vector>
#include <unordered_map>
#include <cstdlib>
#include <cinttypes>
#include <utility>
#include <iostream>



void print_usage() {
  std::cout << "I did not recognize your command! Usage: either:" << std::endl
    << "./cdb_writer.exe dir" << std::endl
    << "or" << std::endl
    << "./cdb_writer.exe file <cdb_file_name>" << std::endl;
}


namespace cdb {
  using namespace std;

  constexpr size_t MAX_STRING_SIZE = 0x1E;

  class CdbField {
  public:


    enum class Inner {
      CDB_CHAR = 0,
      CDB_BOOL = 1, // God knows what it would colide with if it were only "BOOL"
      CDB_I16 = 2,
      CDB_ENUM = 3,
      CDB_I32 = 4,
      CDB_STR
    } value;

    CdbField() = delete;

    size_t size() const {
      if (this->value == Inner::CDB_STR) {
        return cdb::MAX_STRING_SIZE;
      }
      return static_cast<size_t>(this->value);
    }

    explicit CdbField(size_t n) {
      switch (n) {
      case 0:
      case 1:
      case 2:
      case 3:
      case 4:
        this->value = static_cast<CdbField::Inner>(n);
      break; default:
        this->value = Inner::CDB_STR;
        break;
      }
    }
  };

  class CdbStructure {

    CdbStructure() = delete;
  public:
    size_t nfields;
    vector<pair<string, CdbField>> fields;

    CdbStructure(size_t num_fields, vector<string> field_names, vector<CdbField> field_sizes) : nfields(num_fields) {
      assert(field_sizes.size() == field_names.size());
      for (size_t i = 0; i < field_names.size(); i++) {
        this->fields.emplace_back(std::move(field_names[i]), field_sizes[i]);
      }
    }

    size_t size() const {
      size_t res = 0;
      for (size_t i = 0; i < nfields; i++) {
        res += this->fields[i].second.size();
      }
      return res;
    }
  };

  class CdbFileReader {
    FILE* file = nullptr;
    const char* name = nullptr;
  public:

    CdbFileReader() = delete;
    CdbFileReader(const CdbFileReader& other) = delete;

    explicit CdbFileReader(FILE* file, const char* fname) : file(file), name(fname) {}

    char read_char() {
      char c;
      size_t read = fread(&c, sizeof(char), 1, this->file);
      assert(read);
      return c;
    }

    std::string read_str() {
      char buff[cdb::MAX_STRING_SIZE];
      size_t len = cdb::MAX_STRING_SIZE;
      fread(buff, 1, len, this->file);
      string res{};
      for (size_t i = 0; i < len; i++) {
        if (buff[i] != '\0') {
          res += buff[i];
        }
      }
      return res;
    }

    bool read_bool() {
      char c = read_char();
      if (c != 0 && c != 1) {
        cout << "Read a bool which is neither 0 or 1. Something is wrong!";
      }
      return static_cast<bool>(c);
    }

    int16_t read_i16() {
      int16_t bits;
      size_t read = fread(&bits, sizeof(int16_t), 1, this->file);
      assert(read);
      return bits;
    }

    int32_t read_u32_le() {
      int32_t bits;
      size_t read = fread(&bits, sizeof(int32_t), 1, this->file);
      assert(read);
      return bits;
    }

    ~CdbFileReader() {
      if (file != nullptr) {
        fclose(file);
      }
    }

    void read_file() {
      auto type_structure = read_structure();
      long int cur_pos = ftell(this->file);
      long int type_size = static_cast<long int>(type_structure.size());
      assert(fseek(this->file, 0, SEEK_END) == 0); // Make sure the seek succeeded or abort
      long int file_end = ftell(this->file);
      assert(fseek(this->file, cur_pos, SEEK_SET) == 0);  // Make sure the seek succeeded or abort
      if ((file_end - cur_pos) % type_size != 0) {
        cout << "The number of entries in the cdb archive does not divide the size of the type which we were given. Aborting reading file" << this->name << endl;
        return;
      }
      size_t nobjs = (file_end - cur_pos) / type_size;
      for (size_t i = 0; i < nobjs; i++) {
        read_and_output_obj(type_structure);
      }
    }

  private:
    CdbStructure read_structure() {
      uint32_t numfields = read_u32_le();
      size_t nfields = static_cast<size_t>(numfields);

      cout << "Read " << nfields << " fields" << endl;

      vector<string> field_names{};
      for (size_t i = 0; i < nfields; i++) {
        field_names.push_back(std::move(read_str()));
      }
      vector<CdbField> field_types{};
      for (size_t i = 0; i < nfields; i++) {
        field_types.push_back(CdbField(read_u32_le()));
      }

      return CdbStructure(nfields, std::move(field_names), std::move(field_types));
    }

    void read_and_output_obj(const CdbStructure& obj) {
      std::cout << '{' << std::endl;
      for (size_t i = 0; i < obj.nfields; i++) {
        auto& field = obj.fields[i];
        std::cout << "\t\"" << field.first << "\":";

        switch (field.second.value) {
        case CdbField::Inner::CDB_CHAR:
          std::cout << read_char() << std::endl;
          break;
        case CdbField::Inner::CDB_BOOL:
          std::cout << (read_bool() ? "true" : "false");
          break;
        case CdbField::Inner::CDB_I16:
          std::cout << read_i16();
          break;
        case CdbField::Inner::CDB_I32:
          std::cout << read_u32_le();
          break;
        case CdbField::Inner::CDB_STR:
          std::cout << '"' << read_str() << '"';
          break;
        case CdbField::Inner::CDB_ENUM:
        default:
          throw std::runtime_error("Unknown cdb field");
        }

        if (i != obj.nfields - 1) {
          std::cout << ',';
        }
        std::cout << std::endl;
      }
      std::cout << '}' << std::endl;
    }
  };


  void read_file(const char* path) {
    FILE* f = nullptr;
    int res = fopen_s(&f, path, "rb");
    // Keep in mind a return of 0 from fopen_s means success!
    if (res != 0) {
      cout << "Opening of file " << path << " failed!" << endl;
      return;
    }
    CdbFileReader reader(f, path);
    reader.read_file();
  }

};

/* Structure of the cdb file:
   [ number_of_fields: i32,
   number_of_fields x string of length 0x1e,
   number_of_fields x i32 (which represent the types of each field)
   records of the type that the cdb archive represents]
   Keep in mind cdb integers are represented in LITTLE ENDIAN byte order
   */



int main(int argc, char** argv)
{
  if (argc == 1 || (argc == 2 && (strcmp(argv[1], "dir") == 0))) {
    std::cout << "Got it! Trying to load all of the .cdb files from current folder" << std::endl;
  }
  else if (argc == 3 && (strcmp(argv[1], "file") == 0)) {
    std::cout << "Trying to read file " << argv[2] << " as a cdb archive. Output will be saved to the same folder if succesful" << std::endl;
    cdb::read_file(argv[2]);
  }
  else {
    print_usage();
    return EXIT_FAILURE;
  }
  return EXIT_SUCCESS;
}
