#include <windows.h>
#include <commctrl.h>
#include <string>
#include <fstream>
#include <vector>
#include <sstream>
#include <ctime>
#include <cstdlib> // for atof

#pragma comment(lib, "comctl32.lib")

using namespace std;

// Global variables
HWND hMainWnd, hUsername, hPassword, hDisplay, hLoginBtn, hRegisterBtn, 
     hDepositBtn, hWithdrawBtn, hBalanceBtn, hLogoutBtn, hAmount, hAmountLabel;
     
// Admin controls
HWND hAdminUser = NULL;
HWND hAdminAmount = NULL;
HWND hAdminNewPass = NULL;
HWND hAdminActionBtn1 = NULL;
HWND hAdminActionBtn2 = NULL;
HWND hAdminBackBtn = NULL;
HWND hAdminDepositBtn = NULL;
HWND hAdminWithdrawBtn = NULL;
HWND hAdminViewTransBtn = NULL;

// Function prototypes
void HideAllControls();

class User {
private:
    string username;
    string password;
    double balance;
    bool frozen;

public:
    // Constructor
    User() : balance(0.0), frozen(false) {}
    User(string u, string p, double b) : username(u), password(p), balance(b), frozen(false) {}

    // Getters
    string getUsername() const { return username; }
    string getPassword() const { return password; }
    double getBalance() const { return balance; }
    bool isFrozen() const { return frozen; }

    // Setters
    void setPassword(const string& newPass) { password = newPass; }
    void setFrozen(bool status) { frozen = status; }

    // Transaction methods
    bool deposit(double amount) {
        if (amount <= 0) return false;
        balance += amount;
        logTransaction("Deposit", amount);
        return true;
    }

    bool withdraw(double amount) {
        if (amount <= 0 || amount > balance) return false;
        balance -= amount;
        logTransaction("Withdrawal", -amount);
        return true;
    }

    void logTransaction(const string& type, double amount) {
        time_t now = time(0);
        char* dt = ctime(&now);
        
        ofstream logFile(username + "_transactions.txt", ios::app);
        if (logFile.is_open()) {
            logFile << "[" << dt << "] " << type << ": " << (amount >= 0 ? "+" : "") 
                   << amount << " | Balance: " << balance << "\n";
        }
    }

    string getTransactionHistory() const {
        ifstream logFile(username + "_transactions.txt");
        if (!logFile) return "No transactions found.";
        
        string transactions, line;
        while (getline(logFile, line)) {
            transactions += line + "\r\n";
        }
        return transactions.empty() ? "No transactions found." : transactions;
    }
};

class ATM {
private:
    vector<User> users;
    User* currentUser;
    bool isAdmin;
    bool isLoggedIn;

public:
    ATM() : currentUser(nullptr), isAdmin(false), isLoggedIn(false) {
        loadUsers();
    }

    ~ATM() {
        saveUsers();
    }

    // User management
    bool login(const string& username, const string& password) {
        for (auto& user : users) {
            if (user.getUsername() == username && user.getPassword() == password) {
                if (user.isFrozen()) {
                    return false; // Account is frozen
                }
                currentUser = &user;
                isLoggedIn = true;
                isAdmin = (username == "admin");
                return true;
            }
        }
        return false;
    }

    void logout() {
        currentUser = nullptr;
        isLoggedIn = false;
        isAdmin = false;
    }

    bool registerUser(const string& username, const string& password) {
        if (findUser(username) != -1) return false; // User already exists
        users.emplace_back(username, password, 0.0);
        saveUsers();
        return true;
    }

    // Account operations
    bool deposit(double amount) {
        if (!isLoggedIn || !currentUser) return false;
        return currentUser->deposit(amount);
    }

    bool withdraw(double amount) {
        if (!isLoggedIn || !currentUser) return false;
        return currentUser->withdraw(amount);
    }

    string getBalance() const {
        if (!isLoggedIn || !currentUser) return "Not logged in";
        char buffer[100];
        sprintf(buffer, "Current balance: Rs%.2f", currentUser->getBalance());
        return string(buffer);
    }

    // Admin functions
    bool resetPassword(const string& username, const string& newPassword) {
        if (!isAdmin) return false;
        int index = findUser(username);
        if (index == -1) return false;
        users[index].setPassword(newPassword);
        saveUsers();
        return true;
    }

    bool toggleFreezeAccount(const string& username) {
        if (!isAdmin) return false;
        int index = findUser(username);
        if (index == -1) return false;
        users[index].setFrozen(!users[index].isFrozen());
        saveUsers();
        return true;
    }

    string getAllUsers() const {
        if (!isAdmin) return "Access denied";
        string userList = "All Users:\r\n-----------\r\n";
        for (const auto& user : users) {
            if (user.getUsername() != "admin") {
                userList += "User: " + user.getUsername() + " | Balance: Rs" + 
                           to_string(user.getBalance());
                if (user.isFrozen()) userList += " (FROZEN)";
                userList += "\r\n";
            }
        }
        return userList;
    }

    // Getters
    bool isUserLoggedIn() const { return isLoggedIn; }
    bool isUserAdmin() const { return isAdmin; }
    string getCurrentUsername() const { 
        return currentUser ? currentUser->getUsername() : ""; 
    }
    string getTransactionHistory() const {
        return currentUser ? currentUser->getTransactionHistory() : "Not logged in";
    }
    
    // Admin methods
    bool adminDeposit(const string& username, double amount) {
        if (!isAdmin) return false;
        int index = findUser(username);
        if (index == -1 || amount <= 0) return false;
        
        users[index].deposit(amount);
        saveUsers();
        return true;
    }
    
    bool adminWithdraw(const string& username, double amount) {
        if (!isAdmin) return false;
        int index = findUser(username);
        if (index == -1 || amount <= 0) return false;
        
        bool success = users[index].withdraw(amount);
        if (success) saveUsers();
        return success;
    }
    
    string getUserTransactionHistory(const string& username) const {
        if (!isAdmin) return "Access denied";
        int index = findUser(username);
        if (index == -1) return "User not found";
        
        return users[index].getTransactionHistory();
    }
    
    bool isUserFrozen(const string& username) const {
        if (!isAdmin) return false;
        int index = findUser(username);
        return index != -1 && users[index].isFrozen();
    }

private:
    int findUser(const string& username) const {
        for (int i = 0; i < users.size(); i++) {
            if (users[i].getUsername() == username) {
                return i;
            }
        }
        return -1;
    }

    void loadUsers() {
        ifstream file("users.dat");
        if (!file) return;
        
        users.clear();
        string username, password;
        double balance;
        while (file >> username >> password >> balance) {
            User user(username, password, balance);
            users.push_back(user);
        }
    }

    void saveUsers() {
        ofstream file("users.dat");
        for (const auto& user : users) {
            file << user.getUsername() << " " << user.getPassword() 
                 << " " << user.getBalance() << "\n";
        }
    }
};

// Global ATM instance
ATM atm;

// Function to display text in the display area
void DisplayText(const string& text) {
    SetWindowText(hDisplay, text.c_str());
}

// Function to show login screen
void ShowLoginScreen() {
    // First hide all controls
    HideAllControls();
    
    // Show login controls
    ShowWindow(hUsername, SW_SHOW);
    ShowWindow(hPassword, SW_SHOW);
    ShowWindow(hLoginBtn, SW_SHOW);
    ShowWindow(hRegisterBtn, SW_SHOW);
    
    // Clear input fields
    SetWindowText(hUsername, "");
    SetWindowText(hPassword, "");
    SetWindowText(hAmount, "");
    
    DisplayText("Welcome to ATM\r\nPlease login or register.");
}

// Function to create admin controls
void CreateAdminControls() {
    if (hAdminUser) return; // Already created
    
    // Create admin controls
    CreateWindow("STATIC", "Username:", WS_CHILD, 50, 50, 100, 25, hMainWnd, NULL, NULL, NULL);
    hAdminUser = CreateWindow("EDIT", "", WS_CHILD | WS_BORDER, 150, 50, 200, 25, hMainWnd, NULL, NULL, NULL);
    
    CreateWindow("STATIC", "Amount:", WS_CHILD, 50, 90, 100, 25, hMainWnd, NULL, NULL, NULL);
    hAdminAmount = CreateWindow("EDIT", "", WS_CHILD | WS_BORDER, 150, 90, 200, 25, hMainWnd, NULL, NULL, NULL);
    
    CreateWindow("STATIC", "New Password:", WS_CHILD, 50, 130, 100, 25, hMainWnd, NULL, NULL, NULL);
    hAdminNewPass = CreateWindow("EDIT", "", WS_CHILD | WS_BORDER, 150, 130, 200, 25, hMainWnd, NULL, NULL, NULL);
    
    hAdminActionBtn1 = CreateWindow("BUTTON", "Reset Password", WS_CHILD | BS_PUSHBUTTON, 50, 170, 140, 30, hMainWnd, (HMENU)8, NULL, NULL);
    hAdminActionBtn2 = CreateWindow("BUTTON", "Freeze/Unfreeze", WS_CHILD | BS_PUSHBUTTON, 200, 170, 140, 30, hMainWnd, (HMENU)9, NULL, NULL);
    
    hAdminDepositBtn = CreateWindow("BUTTON", "Deposit", WS_CHILD | BS_PUSHBUTTON, 50, 210, 140, 30, hMainWnd, (HMENU)10, NULL, NULL);
    hAdminWithdrawBtn = CreateWindow("BUTTON", "Withdraw", WS_CHILD | BS_PUSHBUTTON, 200, 210, 140, 30, hMainWnd, (HMENU)11, NULL, NULL);
    
    hAdminViewTransBtn = CreateWindow("BUTTON", "View Transactions", WS_CHILD | BS_PUSHBUTTON, 50, 250, 290, 30, hMainWnd, (HMENU)12, NULL, NULL);
    
    hAdminBackBtn = CreateWindow("BUTTON", "Back to Menu", WS_CHILD | BS_PUSHBUTTON, 50, 300, 290, 30, hMainWnd, (HMENU)13, NULL, NULL);
}

// Function to show admin menu
void ShowAdminMenu() {
    if (!atm.isUserAdmin()) return;
    
    // First hide all controls
    HideAllControls();
    
    // Create admin controls if they don't exist
    CreateAdminControls();
    
    // Show admin controls
    ShowWindow(hAdminUser, SW_SHOW);
    ShowWindow(hAdminAmount, SW_SHOW);
    ShowWindow(hAdminNewPass, SW_SHOW);
    ShowWindow(hAdminActionBtn1, SW_SHOW);
    ShowWindow(hAdminActionBtn2, SW_SHOW);
    ShowWindow(hAdminDepositBtn, SW_SHOW);
    ShowWindow(hAdminWithdrawBtn, SW_SHOW);
    ShowWindow(hAdminViewTransBtn, SW_SHOW);
    ShowWindow(hAdminBackBtn, SW_SHOW);
    
    // Clear input fields
    SetWindowText(hAdminUser, "");
    SetWindowText(hAdminAmount, "");
    SetWindowText(hAdminNewPass, "");
    
    // Display user list
    string adminText = "=== ADMIN MENU ===\r\n";
    adminText += atm.getAllUsers();
    
    DisplayText(adminText);
}

// Function to hide all controls
void HideAllControls() {
    // Login controls
    ShowWindow(hUsername, SW_HIDE);
    ShowWindow(hPassword, SW_HIDE);
    ShowWindow(hLoginBtn, SW_HIDE);
    ShowWindow(hRegisterBtn, SW_HIDE);
    
    // Main menu controls
    ShowWindow(hAmount, SW_HIDE);
    ShowWindow(hAmountLabel, SW_HIDE);
    ShowWindow(hDepositBtn, SW_HIDE);
    ShowWindow(hWithdrawBtn, SW_HIDE);
    ShowWindow(hBalanceBtn, SW_HIDE);
    ShowWindow(hLogoutBtn, SW_HIDE);
    
    // Admin controls - only hide if they exist
    if (hAdminUser) {
        ShowWindow(hAdminUser, SW_HIDE);
        ShowWindow(hAdminAmount, SW_HIDE);
        ShowWindow(hAdminNewPass, SW_HIDE);
        ShowWindow(hAdminActionBtn1, SW_HIDE);
        ShowWindow(hAdminActionBtn2, SW_HIDE);
        ShowWindow(hAdminDepositBtn, SW_HIDE);
        ShowWindow(hAdminWithdrawBtn, SW_HIDE);
        ShowWindow(hAdminViewTransBtn, SW_HIDE);
        ShowWindow(hAdminBackBtn, SW_HIDE);
    }
}

// Function to show main menu
void ShowMainMenu(bool fromAdminMenu = false) {
    // First hide all controls
    HideAllControls();
    
    // Show appropriate controls based on user type
    if (atm.isUserAdmin() && !fromAdminMenu) {
        // Show admin menu if user is admin and not coming from admin menu
        ShowAdminMenu();
    } else {
        // Show regular user menu or admin's main menu
        if (atm.isUserAdmin()) {
            // For admin, show admin-specific main menu
            string adminMenu = "=== ADMIN MAIN MENU ===\r\n";
            adminMenu += "1. Deposit\r\n";
            adminMenu += "2. Withdraw\r\n";
            adminMenu += "3. Check Balance\r\n";
            adminMenu += "4. Admin Panel\r\n";
            adminMenu += "5. Logout\r\n";
            DisplayText(adminMenu);
        } else {
            // For regular users
            ShowWindow(hAmount, SW_SHOW);
            ShowWindow(hAmountLabel, SW_SHOW);
            ShowWindow(hDepositBtn, SW_SHOW);
            ShowWindow(hWithdrawBtn, SW_SHOW);
            ShowWindow(hBalanceBtn, SW_SHOW);
            
            string welcome = "Welcome, " + atm.getCurrentUsername() + "!\r\n"
                          + atm.getBalance() + "\r\n"
                          + "Please select an option.";
            DisplayText(welcome);
        }
        
        // Always show logout button
        ShowWindow(hLogoutBtn, SW_SHOW);
    }
}

// Window procedure
LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch(msg) {
        case WM_CREATE: {
            // Create controls
            CreateWindow("STATIC", "Username:", 
                WS_VISIBLE | WS_CHILD,
                50, 50, 100, 25, hwnd, NULL, NULL, NULL);
                
            hUsername = CreateWindow("EDIT", "", 
                WS_VISIBLE | WS_CHILD | WS_BORDER | ES_AUTOHSCROLL,
                150, 50, 200, 25, hwnd, NULL, NULL, NULL);
                
            CreateWindow("STATIC", "Password:", 
                WS_VISIBLE | WS_CHILD,
                50, 90, 100, 25, hwnd, NULL, NULL, NULL);
                
            hPassword = CreateWindow("EDIT", "", 
                WS_VISIBLE | WS_CHILD | WS_BORDER | ES_PASSWORD | ES_AUTOHSCROLL,
                150, 90, 200, 25, hwnd, NULL, NULL, NULL);
                
            hLoginBtn = CreateWindow("BUTTON", "Login", 
                WS_VISIBLE | WS_CHILD | BS_PUSHBUTTON,
                100, 140, 100, 30, hwnd, (HMENU)1, NULL, NULL);
                
            hRegisterBtn = CreateWindow("BUTTON", "Register", 
                WS_VISIBLE | WS_CHILD | BS_PUSHBUTTON,
                220, 140, 100, 30, hwnd, (HMENU)2, NULL, NULL);
                
            hAmountLabel = CreateWindow("STATIC", "Amount:", 
                WS_CHILD,
                50, 90, 100, 25, hwnd, NULL, NULL, NULL);
                
            hAmount = CreateWindow("EDIT", "", 
                WS_CHILD | WS_BORDER | ES_AUTOHSCROLL | ES_NUMBER,
                150, 90, 200, 25, hwnd, NULL, NULL, NULL);
                
            hDepositBtn = CreateWindow("BUTTON", "Deposit", 
                WS_CHILD | BS_PUSHBUTTON,
                50, 140, 100, 30, hwnd, (HMENU)3, NULL, NULL);
                
            hWithdrawBtn = CreateWindow("BUTTON", "Withdraw", 
                WS_CHILD | BS_PUSHBUTTON,
                170, 140, 100, 30, hwnd, (HMENU)4, NULL, NULL);
                
            hBalanceBtn = CreateWindow("BUTTON", "Check Balance", 
                WS_CHILD | BS_PUSHBUTTON,
                50, 190, 220, 30, hwnd, (HMENU)5, NULL, NULL);
                
            hLogoutBtn = CreateWindow("BUTTON", "Logout", 
                WS_CHILD | BS_PUSHBUTTON,
                50, 240, 220, 30, hwnd, (HMENU)6, NULL, NULL);
                
            hDisplay = CreateWindow("EDIT", "", 
                WS_VISIBLE | WS_CHILD | WS_BORDER | ES_MULTILINE | ES_READONLY | WS_VSCROLL | ES_AUTOVSCROLL,
                370, 50, 200, 250, hwnd, NULL, NULL, NULL);
            
            // Show login screen
            ShowLoginScreen();
            break;
        }
        
        case WM_COMMAND: {
            if (LOWORD(wParam) == 1) { // Login
                char username[100], password[100];
                GetWindowText(hUsername, username, 100);
                GetWindowText(hPassword, password, 100);
                
                if (strlen(username) == 0 || strlen(password) == 0) {
                    DisplayText("Error: Please enter both username and password.");
                    break;
                }
                
                if (atm.login(username, password)) {
                    DisplayText("Welcome, " + string(username) + "!");
                    ShowMainMenu();
                } else {
                    DisplayText("Error: Invalid username or password, or account is frozen.");
                }
                break;
            }
            
            else if (LOWORD(wParam) == 2) { // Register
                char username[100], password[100];
                GetWindowText(hUsername, username, 100);
                GetWindowText(hPassword, password, 100);
                
                if (strlen(username) == 0 || strlen(password) == 0) {
                    DisplayText("Error: Please enter both username and password.");
                    break;
                }
                
                if (atm.registerUser(username, password)) {
                    DisplayText("Registration successful! Please login with your new account.");
                } else {
                    DisplayText("Error: Username already exists.");
                }
                break;
            }
            
            else if (LOWORD(wParam) == 3) { // Deposit
                char amountStr[100];
                GetWindowText(hAmount, amountStr, 100);
                double amount = atof(amountStr);
                
                if (amount <= 0) {
                    DisplayText("Error: Please enter a valid amount.");
                    break;
                }
                
                if (atm.deposit(amount)) {
                    string balanceText;
                    double balance = atof(atm.getBalance().c_str());
                    char buffer[100];
                    sprintf(buffer, "Deposit successful!\r\nNew Balance: Rs%.2f", balance);
                    DisplayText(buffer);
                } else {
                    DisplayText("Error: Deposit failed. Please try again.");
                }
                break;
            }
            
            else if (LOWORD(wParam) == 4) { // Withdraw
                char amountStr[100];
                GetWindowText(hAmount, amountStr, 100);
                double amount = atof(amountStr);
                
                if (amount <= 0) {
                    DisplayText("Error: Please enter a valid amount.");
                    break;
                }
                
                if (atm.withdraw(amount)) {
                    string balanceText;
                    double balance = atof(atm.getBalance().c_str());
                    char buffer[100];
                    sprintf(buffer, "Withdrawal successful!\r\nNew Balance: Rs%.2f", balance);
                    DisplayText(buffer);
                } else {
                    DisplayText("Error: Withdrawal failed. Insufficient funds or invalid amount.");
                }
                break;
            }
            
            else if (LOWORD(wParam) == 5) { // Check Balance
                string balanceInfo = "Account Balance\r\n----------------\r\n"
                      "Username: " + atm.getCurrentUsername() + "\r\n"
                      + atm.getBalance();
                DisplayText(balanceInfo);
                break;
            }
            
            else if (LOWORD(wParam) == 6) { // Logout
                atm.logout();
                SetWindowText(hUsername, "");
                SetWindowText(hPassword, "");
                SetWindowText(hAmount, "");
                ShowLoginScreen();
                break;
            }
            
            else if (LOWORD(wParam) == 7) { // Admin Menu
                if (atm.isUserAdmin()) {
                    ShowAdminMenu();
                }
                break;
            }
            
            else if (LOWORD(wParam) >= 8 && LOWORD(wParam) <= 13) { // Admin actions
                if (!atm.isUserAdmin()) {
                    return 0;
                }
                
                char username[100] = {0};
                char amountStr[100] = {0};
                char newPass[100] = {0};
                
                GetWindowText(hAdminUser, username, 100);
                GetWindowText(hAdminAmount, amountStr, 100);
                GetWindowText(hAdminNewPass, newPass, 100);
                
                string userStr(username);
                double amount = atof(amountStr);
                string newPassStr(newPass);
                
                switch (LOWORD(wParam)) {
                    case 8: // Reset Password
                        if (!userStr.empty() && !newPassStr.empty()) {
                            if (atm.resetPassword(userStr, newPassStr)) {
                                DisplayText("Password for " + userStr + " has been reset.");
                            } else {
                                DisplayText("Error: Failed to reset password. User may not exist or insufficient permissions.");
                            }
                        }
                        break;
                        
                    case 9: // Toggle Freeze
                        if (!userStr.empty()) {
                            if (atm.toggleFreezeAccount(userStr)) {
                                string status = atm.isUserFrozen(userStr) ? "frozen" : "unfrozen";
                                DisplayText("Account " + userStr + " has been " + status + ".");
                                ShowAdminMenu(); // Refresh the display
                            } else {
                                DisplayText("Error: Failed to toggle account status. User may not exist or insufficient permissions.");
                            }
                        }
                        break;
                        
                    case 10: // Admin Deposit
                        if (!userStr.empty() && amount > 0) {
                            if (atm.adminDeposit(userStr, amount)) {
                                DisplayText("Deposited Rs." + to_string(amount) + " to " + userStr);
                                ShowAdminMenu();
                            } else {
                                DisplayText("Error: Deposit failed. User may not exist or insufficient permissions.");
                            }
                        }
                        break;
                        
                    case 11: // Admin Withdraw
                        if (!userStr.empty() && amount > 0) {
                            if (atm.adminWithdraw(userStr, amount)) {
                                DisplayText("Withdrawn Rs." + to_string(amount) + " from " + userStr);
                                ShowAdminMenu();
                            } else {
                                DisplayText("Error: Withdrawal failed. Insufficient funds or invalid user/permissions.");
                            }
                        }
                        break;
                        
                    case 12: // View Transactions
                        if (!userStr.empty()) {
                            string transactions = atm.getUserTransactionHistory(userStr);
                            DisplayText("Transactions for " + userStr + ":\r\n" + transactions);
                        }
                        break;
                        
                    case 13: // Back to Menu from Admin
                        ShowMainMenu(true);
                        break;
                }
                
                // Clear input fields
                SetWindowText(hAdminUser, "");
                SetWindowText(hAdminAmount, "");
                SetWindowText(hAdminNewPass, "");
            }
            break;
        }
        
        case WM_DESTROY:
            PostQuitMessage(0);
            return 0;
            
        default:
            return DefWindowProc(hwnd, msg, wParam, lParam);
    }
    return 0;
}

// Entry point
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    // Register window class
    const char CLASS_NAME[] = "ATMClass";
    
    WNDCLASS wc = { };
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = CLASS_NAME;
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    
    RegisterClass(&wc);
    
    // Create the window
    hMainWnd = CreateWindowEx(
        0, CLASS_NAME, "ATM System",
        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
        CW_USEDEFAULT, CW_USEDEFAULT, 600, 400,
        NULL, NULL, hInstance, NULL
    );
    
    if (hMainWnd == NULL) {
        return 0;
    }
    
    ShowWindow(hMainWnd, nCmdShow);
    UpdateWindow(hMainWnd);
    
    // Message loop
    MSG msg = { };
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    
    return 0;
}
