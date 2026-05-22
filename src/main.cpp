#include <iostream>

#include "../include/RadishDB.h"
#include "../include/helpers/Types.h"

class Payload final : public Serializable {
public:
    Payload() = default;

    explicit Payload(std::string username, std::string password, int age, bool isActive)
        : m_username(std::move(username))
        , m_password(std::move(password))
        , m_age(age)
        , m_isActive(isActive)
    {}

    void Serialize(std::ofstream &out) const override {
        const uint32_t usernameLength = static_cast<uint32_t>(m_username.size());
        const uint32_t passwordLength = static_cast<uint32_t>(m_password.size());

        out.write(reinterpret_cast<const char*>(&usernameLength), sizeof(usernameLength));
        out.write(m_username.data(), usernameLength);

        out.write(reinterpret_cast<const char*>(&passwordLength), sizeof(passwordLength));
        out.write(m_password.data(), passwordLength);

        out.write(reinterpret_cast<const char*>(&m_age), sizeof(m_age));
        out.write(reinterpret_cast<const char*>(&m_isActive), sizeof(m_isActive));
    }

    void Deserialize(std::ifstream &in) override {
        uint32_t usernameLength{};
        uint32_t passwordLength{};

        in.read(reinterpret_cast<char*>(&usernameLength), sizeof(usernameLength));

        m_username.resize(usernameLength);
        in.read(m_username.data(), usernameLength);

        in.read(reinterpret_cast<char*>(&passwordLength), sizeof(passwordLength));

        m_password.resize(passwordLength);
        in.read(m_password.data(), passwordLength);

        in.read(reinterpret_cast<char*>(&m_age), sizeof(m_age));
        in.read(reinterpret_cast<char*>(&m_isActive), sizeof(m_isActive));
    }

    void Print() const {
        std::cout << "Username: " << m_username << "\n";
        std::cout << "Password: " << m_password << "\n";
        std::cout << "Age: " << m_age << "\n";
        std::cout << "Is Active: " << (m_isActive ? "Yes" : "No") << "\n";
    }

private:
    std::string m_username;
    std::string m_password;
    int m_age;
    bool m_isActive;
};

int main() {
    Payload user{ "okan", "password123", 30, true };
    user.Print();

    RadishDB<Payload> db("mydb");

    Payload other{};
    other = db.Get("user1").value();
    other.Print();




    return std::cin.get();
}