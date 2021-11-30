
#include <iostream>
#include <assert.h>
#include <numeric>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>
#include <unordered_map>
#include <cstdlib>
#include <cinttypes>
#include <utility>


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
			CDB_U16 = 2,
			CDB_ENUM = 3,
			CDB_U32 = 4,
			CDB_STR // 
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
		size_t nfields;
		vector<pair<string, CdbField>> fields;

		CdbStructure() = delete;
	public:
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
			fread(&c, sizeof(char), 1, this->file);
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

		uint16_t read_u16_le() {
			uint16_t bits;
			fread(&bits, sizeof(uint16_t), 1, this->file);
			char* bytes = reinterpret_cast<char*>(&bits);
			uint16_t res = bytes[0] | (bytes[1] << 8);
			return res;
		}

		uint16_t read_u16_be() {

			return 0;
		}

		uint32_t read_u32_le() {
			uint32_t bits;
			fread(&bits, sizeof(uint32_t), 1, this->file);
			char* bytes = reinterpret_cast<char*>(&bits);
			uint32_t res = bytes[0] | (bytes[1] << 8) | (bytes[2] << 16) | (bytes[3] << 24);
			return res;
		}

		uint32_t read_u32_be() {

			return 0;
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
			if ( (file_end - cur_pos) % type_size != 0) {
				cout << "The number of entries in the cdb archive does not divide the size of the type which we were given. Aborting reading file" << this->name << endl;
				return;
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
	};

	void read_file(const char *path) {
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
 [ number_of_fields: u32, 
	number_of_fields x string of length 0x1e, 
	number_of_fields x u32 (which represent the types of each field)
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
}

