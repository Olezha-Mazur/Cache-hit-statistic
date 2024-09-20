#include <iostream>
#include <bitset>
#include <cinttypes>
#include <list>
#include <unordered_map>
#include <functional>
#include <fstream>
#include <filesystem>
#include <sstream>
#include <ranges>
#include <map>

namespace fs = std::filesystem;

#define MEM_SIZE 524288 
#define ADDR_LEN 19 
#define CACHE_WAY 4
#define CACHE_TAG_LEN 10
#define CACHE_INDEX_LEN 4
#define CACHE_OFFSET_LEN 5
#define CACHE_SIZE 2048
#define CACHE_LINE_SIZE 32 
#define CACHE_LINE_COUNT 64
#define CACHE_SETS 16


int16_t type = 0;
std::vector<uint8_t> memory;

std::map<std::string, uint16_t> types = {
    {"add", 0}, {"sub", 1}, {"xor", 2}, {"or", 3}, {"and", 4}, {"sll", 5}, {"srl", 6}, {"sra", 7}, {"slt", 8}, {"sltu", 9}, 
    {"mul", 10}, {"mulh", 11}, {"mulsu", 12}, {"mulu", 13}, {"div", 14}, {"divu", 15}, {"rem", 16}, {"remu", 17}, //R
    {"addi", 18}, {"xori", 19}, {"ori", 20}, {"andi", 21}, {"slli", 22}, {"srli", 23}, {"srai", 24}, {"slti", 25}, {"sltiu", 26},
    {"lb", 27}, {"lh", 28}, {"lw", 29}, {"lbu", 30}, {"lhu", 31}, {"jalr", 32}, {"ecall", 33}, {"ebreak", 34}, //I
    {"sb", 35}, {"sh", 36}, {"sw", 37}, //S
    {"beq", 38}, {"bne", 39}, {"blt", 40}, {"bge", 41}, {"bltu", 42}, {"bgeu", 43}, //B
    {"jal", 44}, //J
    {"lui", 45}, {"auipc", 46} //U
};

std::map<std::string, uint16_t> opcode = {
    {"add", 51}, {"sub", 51}, {"xor", 51}, {"or", 51}, {"and", 51}, {"sll", 51}, {"srl", 51}, {"sra", 51}, {"slt", 51}, {"sltu", 51}, 
    {"mul", 51}, {"mulh", 51}, {"mulsu", 51}, {"mulu", 51}, {"div", 51}, {"divu", 51}, {"rem", 51}, {"remu", 51}, //R
    {"addi", 19}, {"xori", 19}, {"ori", 19}, {"andi", 19}, {"slli", 19}, {"srli", 19}, {"srai", 19}, {"slti", 19}, {"sltiu", 19},
    {"lb", 3}, {"lh", 3}, {"lw", 3}, {"lbu", 3}, {"lhu", 3}, {"jalr", 103}, {"ecall", 115}, {"ebreak", 115}, //I
    {"sb", 35}, {"sh", 35}, {"sw", 35}, //S
    {"beq", 99}, {"bne", 99}, {"blt", 99}, {"bge", 99}, {"bltu", 99}, {"bgeu", 99}, //B
    {"jal", 111}, //J
    {"lui", 55}, {"auipc", 23}
};

std::map<std::string, uint8_t> func3 = {
    {"add", 0}, {"sub", 0}, {"xor", 4}, {"or", 6}, {"and", 7}, {"sll", 1}, {"srl", 5}, {"sra", 5}, {"slt", 2}, {"sltu", 3}, 
    {"mul", 0}, {"mulh", 1}, {"mulsu", 2}, {"mulu", 3}, {"div", 4}, {"divu", 5}, {"rem", 6}, {"remu", 7}, //R
    {"addi", 0}, {"xori", 4}, {"ori", 6}, {"andi", 7}, {"slli", 1}, {"srli", 5}, {"srai", 5}, {"slti", 2}, {"sltiu", 3},
    {"lb", 0}, {"lh", 1}, {"lw", 2}, {"lbu", 4}, {"lhu", 5}, {"jalr", 0}, {"ecall", 0}, {"ebreak", 0}, //I
    {"sb", 0}, {"sh", 1}, {"sw", 2}, //S
    {"beq", 0}, {"bne", 1}, {"blt", 4}, {"bge", 5}, {"bltu", 6}, {"bgeu", 7}, //B
};

std::map<std::string, uint8_t> func7 = {
    {"add", 0}, {"sub", 32}, {"xor", 0}, {"or", 0}, {"and", 0}, {"sll", 0}, {"srl", 0}, {"sra", 32}, {"slt", 0}, {"sltu", 0}, 
    {"mul", 1}, {"mulh", 1}, {"mulsu", 1}, {"mulu", 1}, {"div", 1}, {"divu", 1}, {"rem", 1}, {"remu", 1}, //R
};

class LRUCache {
public:
    LRUCache() {
        data_.resize(CACHE_SETS);
    }

    void Store(uint32_t address, uint8_t byte) {
        uint16_t tag = (address >> (CACHE_OFFSET_LEN + CACHE_INDEX_LEN));
        uint16_t index = ((address - (tag << (CACHE_OFFSET_LEN + CACHE_INDEX_LEN))) >> (CACHE_OFFSET_LEN));
        ++total_;
        if (data_[index].store(address, byte)) { ++val_try_; }
    }

    uint8_t Load(uint32_t address) {
        uint16_t tag = (address >> (CACHE_OFFSET_LEN + CACHE_INDEX_LEN));
        uint16_t index = ((address - (tag << (CACHE_OFFSET_LEN + CACHE_INDEX_LEN))) >> (CACHE_OFFSET_LEN));
        ++total_;
        auto result = data_[index].load(address);
        if (result.first) { ++val_try_; }
        return result.second;
    }

    void store_statistic() {
        printf("LRU\thit rate: %3.4f%%\n", double(val_try_) / double(total_) * 100);
    }

    size_t total_ = 0;
    size_t val_try_ = 0;

private:
    struct Set {
        std::list<std::pair<uint32_t, std::vector<uint8_t>>> cacheList;
        std::unordered_map<uint32_t, std::list<std::pair<uint32_t, std::vector<uint8_t>>>::iterator> cacheMap;

        Set() {
            cacheList.resize(CACHE_WAY);
        }

        std::pair<bool, uint8_t> load(uint32_t address) {
            uint16_t tag = (address >> (CACHE_OFFSET_LEN + CACHE_INDEX_LEN));
            uint16_t index = (address >> CACHE_OFFSET_LEN) % (1 << CACHE_INDEX_LEN);
            uint16_t offset = address - (address >> (CACHE_OFFSET_LEN) << (CACHE_OFFSET_LEN));
            bool flag = false;
            if (cacheMap.find(tag) != cacheMap.end()) {
                flag = true;
                auto el = *(cacheMap[tag]);
                cacheList.erase(cacheMap[tag]);
                cacheList.push_front({tag, el.second});
                cacheMap[tag] = cacheList.begin();
                return std::make_pair(flag, el.second[offset]);
            }
            if (cacheMap.size() == CACHE_WAY) {
                std::vector<uint8_t> val = cacheList.back().second;
                uint16_t old_tag = cacheList.back().first;
                uint32_t start = (((old_tag << (CACHE_INDEX_LEN)) + index) << (CACHE_OFFSET_LEN));
                uint32_t end = (((old_tag << (CACHE_INDEX_LEN)) + index + 1) << (CACHE_OFFSET_LEN));
                uint32_t ind = 0;
                for (uint32_t i = start; i < end; ++i) {
                    memory[i] = val[ind];
                    ++ind;
                }
                uint32_t leastRecentKey = cacheList.back().first;
                cacheMap.erase(leastRecentKey);
            }

            cacheList.pop_back();

            uint32_t start = (address >> (CACHE_OFFSET_LEN) << (CACHE_OFFSET_LEN));
            uint32_t end = (((address >> (CACHE_OFFSET_LEN)) + 1) << (CACHE_OFFSET_LEN));
            std::vector<uint8_t> value;
            for (uint32_t i = start; i < end; ++i) {
                value.push_back(memory[i]);
            }
            cacheList.push_front({tag, value});
            cacheMap[tag] = cacheList.begin();
            auto el = *(cacheMap[tag]);
            return std::make_pair(flag, el.second[offset]);
        }

        bool store(uint32_t address, uint8_t byte) {
            uint16_t tag = (address >> (CACHE_OFFSET_LEN + CACHE_INDEX_LEN));
            uint16_t index = (address >> CACHE_OFFSET_LEN) % (1 << CACHE_INDEX_LEN);

            uint16_t offset = address - (address >> (CACHE_OFFSET_LEN) << (CACHE_OFFSET_LEN));
            if (cacheMap.find(tag) != cacheMap.end()) {
                cacheMap[tag]->second[offset] = byte;
                auto line = *(cacheMap[tag]);
                cacheList.erase(cacheMap[tag]);
                cacheList.push_front({tag, line.second});
                cacheMap[tag] = cacheList.begin();
                return true;
            }

            if (cacheMap.size() == CACHE_WAY) {
                std::vector<uint8_t> val = cacheList.back().second;
                uint16_t old_tag = cacheList.back().first;
                uint32_t start = (((old_tag << (CACHE_INDEX_LEN)) + index) << (CACHE_OFFSET_LEN));
                uint32_t end = (((old_tag << (CACHE_INDEX_LEN)) + index + 1) << (CACHE_OFFSET_LEN));
                uint32_t ind = 0;
                for (uint32_t i = start; i < end; ++i) {
                    memory[i] = val[ind];
                    ++ind;
                }
            }
            uint32_t leastRecentKey = cacheList.back().first;
            cacheMap.erase(leastRecentKey);
            cacheList.pop_back();

            std::vector<uint8_t> value;
            uint32_t start = (address >> (CACHE_OFFSET_LEN) << (CACHE_OFFSET_LEN));
            uint32_t end = (((address >> (CACHE_OFFSET_LEN)) + 1) << (CACHE_OFFSET_LEN));
            for (uint32_t i = start; i < end; ++i) {
                value.push_back(memory[i]);
            }
            cacheList.push_front({tag, value});
            cacheMap[tag] = cacheList.begin();
            cacheMap[tag]->second[offset] = byte;
            return false;
        }
    };
    std::vector<Set> data_;
};

class bitLRUCache {
public:
    bitLRUCache() {
        data_.resize(CACHE_SETS);
    }

    void Store(uint32_t address, uint8_t byte) {
        uint16_t tag = (address >> (CACHE_OFFSET_LEN + CACHE_INDEX_LEN));
        uint16_t index = ((address - (tag << (CACHE_OFFSET_LEN + CACHE_INDEX_LEN))) >> (CACHE_OFFSET_LEN));
        ++total_;
        if (data_[index].store(address, byte)) { ++val_try_; }
    }

    uint8_t Load(uint32_t address) {
        uint16_t tag = (address >> (CACHE_OFFSET_LEN + CACHE_INDEX_LEN));
        uint16_t index = ((address - (tag << (CACHE_OFFSET_LEN + CACHE_INDEX_LEN))) >> (CACHE_OFFSET_LEN));
        ++total_;
        auto result = data_[index].load(address);
        if (result.first) { ++val_try_; }
        return result.second;
    }

    void store_statistic() {
        printf("pLRU\thit rate: %3.4f%%\n", double(val_try_) / double(total_) * 100);
    }

    size_t total_ = 0;
    size_t val_try_ = 0;

private:
    struct CacheLine {
        bool mru;
        uint16_t tag = 0xFFFF;
        std::vector<uint8_t> data;
    };

    struct Set {
        std::vector<CacheLine> cacheList;

        int16_t Find(uint16_t tag) {
            for (int16_t i = 0; i < cacheList.size(); ++i) {
                if (cacheList[i].tag == tag) {
                    return i;
                }
            }
            return -1;
        }

        int16_t FindFirst() {
            for (int16_t i = 0; i < cacheList.size(); ++i) {
                if (!cacheList[i].mru) {
                    return i;
                }
            }
            return -1;
        }

        void Fill() {
            if (FindFirst() == -1) {
                for (size_t i = 0; i < cacheList.size(); ++i) {
                    cacheList[i].mru = false;
                }
            }
        }

        void LoadLine(uint32_t address, std::vector<uint8_t>& value) {
            uint32_t start = (address >> (CACHE_OFFSET_LEN) << (CACHE_OFFSET_LEN));
            uint32_t end = (((address >> (CACHE_OFFSET_LEN)) + 1) << (CACHE_OFFSET_LEN));
            for (uint32_t i = start; i < end; ++i) {
                value.push_back(memory[i]);
            }
        }

        std::pair<bool, uint8_t> load(uint32_t address) {
            uint16_t tag = (address >> (CACHE_OFFSET_LEN + CACHE_INDEX_LEN));
            uint16_t index = (address >> CACHE_OFFSET_LEN) % (1 << CACHE_INDEX_LEN);
            uint16_t offset = address - (address >> (CACHE_OFFSET_LEN) << (CACHE_OFFSET_LEN));
            int16_t ind = Find(tag);
            if (ind != -1) {
                cacheList[ind].mru = true;
                if (cacheList.size() == CACHE_WAY) {
                    Fill();
                    cacheList[ind].mru = true;
                }
                return std::make_pair(true, cacheList[ind].data[offset]);
            }
            std::vector<uint8_t> value;
            LoadLine(address, value);
            if (cacheList.size() != CACHE_WAY) {
                cacheList.push_back(CacheLine{true, tag, value});
                if (cacheList.size() == CACHE_WAY) {
                    Fill();
                    cacheList.back().mru = true;
                }
                return std::make_pair(false, value[offset]);
            }
            ind = FindFirst();
            uint32_t old_tag = cacheList[ind].tag;
            std::vector<uint8_t> old_value = cacheList[ind].data;
            cacheList[ind].mru = true;
            cacheList[ind].tag = tag;
            cacheList[ind].data = value;
            Fill();
            cacheList[ind].mru = true;
            uint32_t start = (((old_tag << (CACHE_INDEX_LEN)) + index) << (CACHE_OFFSET_LEN));
            uint32_t end = (((old_tag << (CACHE_INDEX_LEN)) + index + 1) << (CACHE_OFFSET_LEN));
            uint32_t k = 0;
            for (uint32_t i = start; i < end; ++i) {
                memory[i] = old_value[k];
                ++k;
            }
            return std::make_pair(false, value[offset]);
        }

        bool store(uint32_t address, uint8_t byte) {
            uint16_t tag = (address >> (CACHE_OFFSET_LEN + CACHE_INDEX_LEN));
            uint16_t index = (address >> CACHE_OFFSET_LEN) % (1 << CACHE_INDEX_LEN);
            uint16_t offset = address - (address >> (CACHE_OFFSET_LEN) << (CACHE_OFFSET_LEN));
            int16_t ind = Find(tag);
            if (ind != -1) {
                cacheList[ind].mru = true;
                if (cacheList.size() == CACHE_WAY) {
                    Fill();
                    cacheList[ind].mru = true;
                }
                cacheList[ind].data[offset] = byte;
                return true;
            }
            std::vector<uint8_t> value;
            LoadLine(address, value);
            value[offset] = byte;
            if (cacheList.size() != CACHE_WAY) {
                cacheList.push_back(CacheLine{true, tag, value});
                if (cacheList.size() == CACHE_WAY) {
                    Fill();
                    cacheList.back().mru = true;
                }
                return false;
            }
            ind = FindFirst();
            uint32_t old_tag = cacheList[ind].tag;
            std::vector<uint8_t> old_value = cacheList[ind].data;
            cacheList[ind].mru = true;
            cacheList[ind].tag = tag;
            cacheList[ind].data = value;
            Fill();
            cacheList[ind].mru = true;
            uint32_t start = (((old_tag << (CACHE_INDEX_LEN)) + index) << (CACHE_OFFSET_LEN));
            uint32_t end = (((old_tag << (CACHE_INDEX_LEN)) + index + 1) << (CACHE_OFFSET_LEN));
            uint32_t k = 0;
            for (uint32_t i = start; i < end; ++i) {
                memory[i] = old_value[k];
                ++k;
            }
            return false;
        }
    };
    std::vector<Set> data_;
};

std::vector<uint32_t> reg;

std::map<std::string, uint16_t> registers = {
    {"zero", 0}, {"ra", 1}, {"sp", 2}, {"gp", 3}, {"tp", 4}, {"t0", 5}, {"t1", 6}, {"t2", 7},
    {"s0", 8}, {"fp", 8}, {"s1", 9}, {"a0", 10}, {"a1", 11}, {"a2", 12}, {"a3", 13}, {"a4", 14}, {"a5", 15},
    {"a6", 16}, {"a7", 17}, {"s2", 18}, {"s3", 19}, {"s4", 20}, {"s5", 21}, {"s6", 22}, {"s7", 23},
    {"s8", 24}, {"s9", 25}, {"s10", 26}, {"s11", 27}, {"t3", 28}, {"t4", 29}, {"t5", 30}, {"t6", 31}
};

void toLowerCase(std::string& str) {
    std::locale loc;
    for (int i = 0; i < str.length(); i++) {
        str[i] = std::tolower(str[i], loc);
    }
}

struct Instruction {
    std::string command;
    uint32_t rd;
    uint32_t rs1;
    uint32_t rs2;
    uint32_t imm;

    void show() { std::cout << command << ' ' << rd << ' ' << rs1 << ' ' << rs2 << ' ' << imm << '\n'; }
};

uint32_t ToInt(std::string line) {
    std::istringstream iss(line);
    std::string token;
    iss >> token;
    if (token.find("0x") != std::string::npos) {
        uint32_t num;
        std::stringstream ss;
        ss << std::hex << token;
        ss >> num;
        return num;
    } else {
        return std::stoul(token, nullptr, 10);
    }
} 

std::pair<std::string, std::string> SplitString(std::string elem) {
    size_t ind = 0;
    while (elem[ind] != '(') {
        ++ind;
    }
    std::string f = elem.substr(0, ind);
    std::string s = elem.substr(ind + 1, elem.size() - f.size() - 2);
    return make_pair(f, s);
}

void ReadInstructions(std::ifstream& file, std::vector<Instruction>& arr) {
    std::string line;
    std::vector<std::string> lines;
    bool flag = true;
    while (file >> line) {
        toLowerCase(line);
        if (line == "//") flag = false;
        if (types.contains(line)) flag = true;
        if (line.back() == ',') {
            line = line.substr(0, line.size() - 1);
        }
        if (flag) lines.push_back(line);
    }
    size_t ind = 0;
    while (ind < lines.size()) {
        auto command = types[lines[ind]];
        Instruction inst;
        inst.command = lines[ind];
        if (0 <= command && command <= 17) {
            inst.rd = registers[lines[ind + 1]];
            inst.rs1 = registers[lines[ind + 2]];
            inst.rs2 = registers[lines[ind + 3]];
            ind += 4;
        } else if ((18 <= command && command <= 26) || (command == 32)) {
            inst.rd = registers[lines[ind + 1]];
            inst.rs1 = registers[lines[ind + 2]];
            inst.imm = ToInt(lines[ind + 3]);
            ind += 4;
        } else if ((27 <= command && command <= 31)) {
            inst.rd = registers[lines[ind + 1]];
            inst.imm = ToInt(lines[ind + 2]);
            inst.rs1 = registers[lines[ind + 3]];
            ind += 4;
        } else if ((38 <= command && command <= 43)) {
            inst.rs1 = registers[lines[ind + 1]];
            inst.rs2 = registers[lines[ind + 2]];
            inst.imm = ToInt(lines[ind + 3]);
            ind += 4;
        } else if (44 <= command && command <= 46) {
            inst.rd = registers[lines[ind + 1]];
            inst.imm = ToInt(lines[ind + 2]);
            ind += 3;
        } else if (((35 <= command && command <= 37))) {
            inst.rs2 = registers[lines[ind + 1]];
            inst.imm = ToInt(lines[ind + 2]);
            inst.rs1 = registers[lines[ind + 3]];
            ind += 4;
        }
        arr.push_back(inst);
    }
}

int32_t Negation(uint32_t elem) {
    uint64_t sz = 1;
    uint64_t mod = (uint64_t)(sz << 32);
    if (elem & (1 << (32 - 1))) {
        return static_cast<int32_t>(elem - mod);
    } else {
        return static_cast<int32_t>(elem);
    }
}

template <typename T>
void Simulate(std::vector<Instruction>& arr, T& cache) {
    uint64_t mul;
    size_t val_try;
    uint32_t PC = 0;
    uint32_t address;
    while (PC < arr.size()) {
        bool flag = true;
        auto curr = arr[PC];
        switch (types[curr.command]) {
        case 0:
            reg[curr.rd] = reg[curr.rs1] + reg[curr.rs2];
            break;
        case 1:
            reg[curr.rd] = reg[curr.rs1] - reg[curr.rs2];
            break;
        case 2:
            reg[curr.rd] = reg[curr.rs1] ^ reg[curr.rs2];
            break;
        case 3:
            reg[curr.rd] = reg[curr.rs1] | reg[curr.rs2];
            break;
        case 4:
            reg[curr.rd] = reg[curr.rs1] & reg[curr.rs2];
            break;
        case 5:
            reg[curr.rd] = reg[curr.rs1] << reg[curr.rs2];
            break;
        case 6:
            reg[curr.rd] = reg[curr.rs1] >> reg[curr.rs2];
            break;
        case 7:
            reg[curr.rd] = reg[curr.rs1] >> reg[curr.rs2];
            break;
        case 8:
            reg[curr.rd] = (reg[curr.rs1] < reg[curr.rs2]) ? 1 : 0;
            break;
        case 9:
            reg[curr.rd] = (reg[curr.rs1] < reg[curr.rs2]) ? 1 : 0;
            break;
        case 10:
            mul = reg[curr.rs1] * reg[curr.rs2];
            reg[curr.rd] = (uint32_t)(mul - ((mul >> 32) << 32));
            break;
        case 11:
            mul = reg[curr.rs1] * reg[curr.rs2];
            reg[curr.rd] = (uint32_t)(mul >> 32);
            break;
        case 12:
            mul = reg[curr.rs1] * reg[curr.rs2];
            reg[curr.rd] = (uint32_t)(mul >> 32);
            break;
        case 13:
            mul = reg[curr.rs1] * reg[curr.rs2];
            reg[curr.rd] = (uint32_t)(mul >> 32);
            break;
        case 14:
            reg[curr.rd] = reg[curr.rs1] / reg[curr.rs2];
            break;
        case 15:
            reg[curr.rd] = reg[curr.rs1] / reg[curr.rs2];
            break;
        case 16:
            reg[curr.rd] = reg[curr.rs1] % reg[curr.rs2];
            break;
        case 17:
            reg[curr.rd] = reg[curr.rs1] % reg[curr.rs2];
            break;
        case 18:
            reg[curr.rd] = reg[curr.rs1] + Negation(curr.imm);
            break;
        case 19:
            reg[curr.rd] = reg[curr.rs1] ^ Negation(curr.imm);
            break;
        case 20:
            reg[curr.rd] = reg[curr.rs1] | Negation(curr.imm);
            break;
        case 21:
            reg[curr.rd] = reg[curr.rs1] & Negation(curr.imm);
            break;
        case 22:
            reg[curr.rd] = (reg[curr.rs1] << Negation(curr.imm));
            break;
        case 23:
            reg[curr.rd] = reg[curr.rs1] >> Negation(curr.imm);
            break;
        case 24:
            reg[curr.rd] = reg[curr.rs1] >> Negation(curr.imm);
            break;
        case 25:
            reg[curr.rd] = (reg[curr.rs1] < curr.imm) ? 1 : 0;
            break;
        case 26:
            reg[curr.rd] = (reg[curr.rs1] < curr.imm) ? 1 : 0;
            break;
        case 27:
            reg[curr.rd] = cache.Load(reg[curr.rs1] + Negation(curr.imm));
            break;
        case 28:
            val_try = cache.val_try_;
            address = reg[curr.rs1] + Negation(curr.imm);
            reg[curr.rd] = cache.Load(address);
            reg[curr.rd] <<= 8;
            reg[curr.rd] = cache.Load(address + 1);
            if ((cache.val_try_ - val_try) != 2) {
                cache.val_try_ = val_try;
            } else {
                cache.val_try_ = val_try + 1;
            }
            cache.total_ -= 1;
            break;
        case 29:
            val_try = cache.val_try_;
            address = reg[curr.rs1] + Negation(curr.imm);
            for (int i = 0; i < 4; ++i) {
                reg[curr.rd] = cache.Load(address + i);
                if (i != 3) reg[curr.rd] <<= 8;
            }
            if ((cache.val_try_ - val_try) != 4) {
                cache.val_try_ = val_try;
            } else {
                cache.val_try_ = val_try + 1;
            }
            cache.total_ -= 3;
            break;
        case 30:
            reg[curr.rd] = cache.Load(reg[curr.rs1] + Negation(curr.imm));
            break;
        case 31:
            val_try = cache.val_try_;
            address = reg[curr.rs1] + Negation(curr.imm);
            reg[curr.rd] = cache.Load(address);
            reg[curr.rd] <<= 8;
            reg[curr.rd] = cache.Load(address + 1);
            if ((cache.val_try_ - val_try) != 2) {
                cache.val_try_ = val_try;
            } else {
                cache.val_try_ = val_try + 1;
            }
            cache.total_ -= 1;
            break;
        case 35:
            cache.Store(reg[curr.rs1] + Negation(curr.imm), reg[curr.rs2] % (1 << 8));
            break;
        case 36:
            val_try = cache.val_try_;
            address = reg[curr.rs1] + Negation(curr.imm);
            cache.Store(address, (reg[curr.rs2] >> 8) % (1 << 8));
            cache.Store(address + 1, reg[curr.rs2] % (1 << 8));
            if ((cache.val_try_ - val_try) != 2) {
                cache.val_try_ = val_try;
            } else {
                cache.val_try_ = val_try + 1;
            }
            cache.total_ -= 1;
            break;
        case 37:
            val_try = cache.val_try_;
            address = reg[curr.rs1] + Negation(curr.imm);
            for (int i = 0; i < 4; ++i) {
                cache.Store(address + i, (reg[curr.rs2] >> (i * 8)) % (1 << 8));
            }
            if ((cache.val_try_ - val_try) != 4) {
                cache.val_try_ = val_try;
            } else {
                cache.val_try_ = val_try + 1;
            }
            cache.total_ -= 3;
            break;
        case 38:
            if (reg[curr.rs1] == reg[curr.rs2]) {
                PC += (Negation(curr.imm) / 4);
                flag = false;
            }
            break;
        case 39:
            if (reg[curr.rs1] != reg[curr.rs2]) {
                PC += (Negation(curr.imm) / 4);
                flag = false;
            }
            break;
        case 40:
            if (reg[curr.rs1] < reg[curr.rs2]) {
                PC += (Negation(curr.imm) / 4);
                flag = false;
            }
            break;
        case 41:
            if (reg[curr.rs1] >= reg[curr.rs2]) {
                PC += (Negation(curr.imm) / 4);
                flag = false;
            }
            break;
        case 42:
            if (reg[curr.rs1] < reg[curr.rs2]) {
                PC += (Negation(curr.imm) / 4);
                flag = false;
            }
            break;
        case 43:
            if (reg[curr.rs1] >= reg[curr.rs2]) {
                PC += (Negation(curr.imm) / 4);
                flag = false;
            }
            break;
        case 32:
            reg[curr.rd] = PC + 4;
            PC += ((Negation(curr.imm) / 4) + curr.rs1);
            if (curr.rd == 0) {
                reg[0] = 0;
                PC = static_cast<uint32_t>(arr.size());
            }
            flag = false;
            break;
        case 44:
            curr.rd = PC + 4;
            PC += ((Negation(curr.imm) / 4));
            flag = false;
            break;
        case 45:
            curr.rd = ((Negation(curr.imm)) << 12);
            break;
        case 46:
            curr.rd = ((Negation(curr.imm)) << 12) + PC * 4;
            break;
        default:
            break;
        }
        if (flag) { ++PC; }
    }
}

std::vector<Instruction> arr;

void ASM(const std::string& asm_file) {
    fs::path dir(asm_file);
    std::ifstream file;
    file.open(dir, std::ios::app | std::ios::binary);
    size_t pos = file.tellg();

    ReadInstructions(file, arr);
    if (type == 1) {
        LRUCache cache;
        Simulate(arr, cache);
        cache.store_statistic();
    } else if (type == 2) {
        bitLRUCache cache;
        Simulate(arr, cache);
        cache.store_statistic();
    } else {
        LRUCache cache1;
        Simulate(arr, cache1);
        cache1.store_statistic();
        bitLRUCache cache2;
        Simulate(arr, cache2);
        cache2.store_statistic();
    }
}

void BIN(const std::string& bin_file) {
    fs::path dir(bin_file);
    std::ofstream file;
    file.open(dir, std::ios::binary);
    for (auto elem: arr) {
        uint32_t line = 0;
        uint32_t command = types[elem.command];
        if ((0 <= command && command <= 17)) {
            line += func7[elem.command];
            line <<= 5;
            line += elem.rs2;
            line <<= 5;
            line += elem.rs1;
            line <<= 3;
            line += func3[elem.command];
            line <<= 5;
            line += elem.rd;
        } else if ((18 <= command && command <= 34)) { 
            line += (elem.imm - (elem.imm >> 12 << 12));
            line <<= 5;
            line += elem.rs1;
            line <<= 3;
            line += func3[elem.command];
            line <<= 5;
            line += elem.rd;
        } else if ((35 <= command && command <= 37)) { // S
            line += ((elem.imm >> 5) - ((elem.imm >> 5) >> 7 << 7));
            line <<= 5;
            line += elem.rs2;
            line <<= 5;
            line += elem.rs1;
            line <<= 3;
            line += func3[elem.command];
            line <<= 5;
            line += (elem.imm - (elem.imm >> 5 << 5));
        } else if ((38 <= command && command <= 43)) { // B
            line += (elem.imm >> 12) % 2;
            line <<= 6;
            line += ((elem.imm >> 5) - (elem.imm >> 11 << 6));
            line <<= 5;
            line += elem.rs2;
            line <<= 5;
            line += elem.rs1;
            line <<= 3;
            line += func3[elem.command];
            line <<= 4;
            line += ((elem.imm >> 1) - (elem.imm >> 5 << 4));
            line <<= 1;
            line += ((elem.imm >> 11) % 2);
        } else if (command == 44) { // J
            line += (elem.imm >> 20) % 2;
            line <<= 10;
            line += ((elem.imm >> 1) - (elem.imm >> 11 << 10));
            line <<= 1;
            line += (elem.imm >> 11) % 2;
            line <<= 8;
            line += ((elem.imm >> 12) - (elem.imm >> 20 << 8));
            line <<= 5;
            line += elem.rd;
        } else if (command == 45 || command == 46) { // U
            line += ((elem.imm >> 12) - ((elem.imm >> 12) >> 20 << 20));
            line <<= 5;
            line += elem.rd;
        } else { 
            line <<= 5;
            line += elem.rd;
        }
        line <<= 7;
        line += opcode[elem.command];
        for (int i = 0; i < 4; ++i) {
            uint8_t byte = line - (line >> 8 << 8);
            file << byte;
            line >>= 8;
        }
    }
}

bool Parse(int argc, char** argv) {
    std::string asm_file;
    std::string bin_file;
    for (size_t i = 1; i < argc; ++i) {
        std::string elem = argv[i];
        if (elem == "--replacement") {
            type = (argv[i + 1][0] - '0');
        } else if (elem == "--asm") {
            asm_file = (argv[i + 1]);
        } else if (elem == "--bin") {
            bin_file = (argv[i + 1]);
        } else {
            continue;
        }
    }
    ASM(asm_file);
    if (bin_file != "") BIN(bin_file);
    return true;
}

int main(int argc, char** argv) {
    memory.resize(MEM_SIZE);
    reg.resize(32);
    reg[0] = 0;
    Parse(argc, argv);
    return 0;
}