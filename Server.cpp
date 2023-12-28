#include <iostream>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>
#include <ctime>
#include <iomanip>
#include <sstream>
#include <algorithm>

std::string generate_commit_hash(const std::string &message) {
    return std::to_string(std::hash<std::string>{}(message));
}

bool init() {
    try {
        std::filesystem::create_directory(".git");
        std::filesystem::create_directory(".git/objects");
        std::filesystem::create_directory(".git/refs");
        std::filesystem::create_directory(".git/refs/heads");

        std::ofstream headFile(".git/HEAD");
        if (headFile.is_open()) {
            headFile << "ref: refs/heads/master\n";
            headFile.close();
        } else {
            std::cerr << "Failed to create .git/HEAD file.\n";
            return false;
        }

        std::cout << "Initialized git directory\n";
    } catch (const std::filesystem::filesystem_error &e) {
        std::cerr << e.what() << '\n';
        return false;
    }
    return true;
}

// ...

bool add() {
    try {
        // List of file extensions to exclude
        std::vector<std::string> excludedExtensions = {".json"};

        // Iterate through the current directory and its subdirectories
        for (const auto &entry : std::filesystem::recursive_directory_iterator(".")) {
            // Skip files in the .git directory
            if (entry.path().string().find(".git") != std::string::npos) {
                continue;
            }

            // Check if the entry is a regular file
            if (entry.is_regular_file()) {
                // Check if the file has an excluded extension
                std::string fileExtension = entry.path().extension().string();
                if (std::find_if(excludedExtensions.begin(), excludedExtensions.end(),
                                 [&fileExtension](const std::string &ext) { return fileExtension == ext; }) != excludedExtensions.end()) {
                    continue;
                }

                // Read the file content, skip if the file cannot be read
                std::ifstream file(entry.path(), std::ios::binary);
                if (!file.is_open()) {
                    std::cerr << "Could not read file " << entry.path() << ". Skipping...\n";
                    continue;
                }

                std::string fileContent((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());

                // Calculate the hash of the file content
                std::string fileHash = generate_commit_hash(fileContent);

                // Write the file content to the objects directory
                std::filesystem::path filePath = entry.path();
                filePath = ".git/objects/" + fileHash.substr(0, 2) + "/" + fileHash.substr(2);
                std::ofstream fileObject(filePath, std::ios::out | std::ios::binary);

                if (!fileObject.is_open()) {
                    std::cerr << "Failed to create file object for " << entry.path() << ". Skipping...\n";
                    continue;
                }

                fileObject.write(fileContent.c_str(), fileContent.size());
                fileObject.close();
            }
        }

        std::cout << "Staged changes\n";
    } catch (const std::filesystem::filesystem_error &e) {
        std::cerr << "Error staging changes: " << e.what() << '\n';
        return false;
    }
    return true;
}







bool create_branch(const std::string &branchName) {
    try {
        // Create the branch file without specifying a commit hash
        std::filesystem::path branchesPath = ".git/refs/heads";
        std::filesystem::path branchPath = branchesPath / branchName;

        if (!std::filesystem::exists(branchesPath) && !std::filesystem::create_directories(branchesPath)) {
            std::cerr << "Failed to create branch directory " << branchesPath << '\n';
            return false;
        }

        std::ofstream branchFile(branchPath);
        if (branchFile.is_open()) {
            branchFile.close();
            std::cout << "Created branch " << branchName << "\n";
        } else {
            std::cerr << "Failed to create branch " << branchName << '\n';
            return false;
        }
    } catch (const std::filesystem::filesystem_error &e) {
        std::cerr << e.what() << '\n';
        return false;
    }
    return true;
}


bool switch_branch(const std::string &branchName) {
    try {
        std::ofstream headFile(".git/HEAD");
        if (headFile.is_open()) {
            headFile << "ref: refs/heads/" << branchName << "\n";
            headFile.close();
        } else {
            std::cerr << "Failed to switch to branch " << branchName << '\n';
            return false;
        }

        std::cout << "Switched to branch " << branchName << "\n";
    } catch (const std::filesystem::filesystem_error &e) {
        std::cerr << e.what() << '\n';
        return false;
    }
    return true;
}

std::vector<std::string> list_branches() {
    std::vector<std::string> branches;
    std::filesystem::path branchesPath = ".git/refs/heads";

    try {
        for (const auto& entry : std::filesystem::directory_iterator(branchesPath)) {
            if (entry.is_regular_file()) {
                branches.push_back(entry.path().filename().string());
            }
        }
    } catch (const std::filesystem::filesystem_error& e) {
        std::cerr << e.what() << '\n';
    }

    return branches;
}

bool show_branches() {
    std::vector<std::string> branches = list_branches();

    if (!branches.empty()) {
        std::cout << "List of branches:\n";
        for (const std::string& branch : branches) {
            std::cout << "- " << branch << '\n';
        }
    } else {
        std::cout << "No branches found.\n";
    }

    return true;
}

bool commit(const std::string &message) {
    try {
        // Get the current commit hash
        std::ifstream headFile(".git/HEAD");
        std::string headContent;
        if (headFile.is_open()) {
            std::getline(headFile, headContent);
            headFile.close();
        }
        std::string currentCommitHash = headContent.substr(headContent.find("commit") + 7);

        // Simplified commit format: commit message
        std::string commitContent = "tree " + currentCommitHash + "\nparent " + currentCommitHash + "\nauthor <your_name> <<your_email@example.com>> <timestamp>\ncommitter <your_name> <<your_email@example.com>> <timestamp>\n\n" + message;

        // Write the commit object to the objects directory
        std::string commitHash = generate_commit_hash(commitContent);
        std::filesystem::path commitPath = ".git/objects/" + commitHash.substr(0, 2) + "/" + commitHash.substr(2);
        std::cout << "Commit file path: " << commitPath << "\n";  // Debug print

        // Create the directories for the commit object if they don't exist
        std::filesystem::create_directories(commitPath.parent_path());

        std::ofstream commitFile(commitPath, std::ios::out | std::ios::binary);

        if (!commitFile.is_open()) {
            std::cerr << "Failed to create commit object. Could not open commit file.\n";
            return false;
        }

        commitFile.write(commitContent.c_str(), commitContent.size());
        commitFile.close();

        // Update the HEAD reference
        std::ofstream updatedHeadFile(".git/HEAD");
        if (updatedHeadFile.is_open()) {
            updatedHeadFile << "ref: refs/heads/master\n";
            updatedHeadFile << "commit " << commitHash << "\n";
            updatedHeadFile.close();
        } else {
            std::cerr << "Failed to update .git/HEAD reference.\n";
            return false;
        }

        std::cout << "Committed changes\n";
    } catch (const std::filesystem::filesystem_error &e) {
        std::cerr << "Error creating commit object: " << e.what() << '\n';
        return false;
    } catch (const std::exception &e) {
        std::cerr << "Error creating commit object: " << e.what() << '\n';
        return false;
    }
    return true;
}






std::string get_parent_commit(const std::string &commitHash) {
    std::ifstream file(".git/objects/" + commitHash.substr(0, 2) + "/" + commitHash.substr(2), std::ios::binary);
    if (!file.is_open()) {
        std::cerr << "Could not read object " << commitHash << '\n';
        return "";
    }

    // Assuming the object is a commit for simplicity
    std::string commitContent((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());

    size_t pos = commitContent.find("parent ");
    if (pos != std::string::npos) {
        return commitContent.substr(pos + 7, 40); // Assuming parent commit hash is 40 characters
    }

    return "";
}
// Function to retrieve commit message given the commit hash
std::string get_commit_message(const std::string &commitHash) {
    std::string folder = commitHash.substr(0, 2);
    std::string f = commitHash.substr(2);

    std::filesystem::path path = ".git/objects";
    path /= folder;
    path /= f;

    std::ifstream file(path, std::ios::binary);
    if (!file.is_open()) {
        std::cerr << "Could not read object " << commitHash << '\n';
        return "";
    }

    // Assuming the object is a commit for simplicity
    std::string commitContent((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    return commitContent;
}


void show_commit_history() {
    std::ifstream headFile(".git/HEAD");
    if (headFile.is_open()) {
        std::string line;
        std::getline(headFile, line); // Read the first line in HEAD file
        if (line.find("ref: refs/heads/master") == std::string::npos) {
            std::cerr << "Invalid format in HEAD file.\n";
            return;
        }

        while (std::getline(headFile, line)) {
            if (line.find("commit") != std::string::npos) {
                std::string commitHash = line.substr(line.find("commit") + 7);
                std::cout << "Commit: " << commitHash << "\n";
                std::string commitMessage = get_commit_message(commitHash);
                std::cout << "Message: " << commitMessage << "\n";
                std::cout << "--------------------------------\n";
            }
        }

        headFile.close();
    }
}


int main(int argc, char *argv[]) {
    if (argc < 2) {
        std::cerr << "No command provided.\n";
        return EXIT_FAILURE;
    }

    std::string command = argv[1];

    if (command == "init") {
        init();
    } else if (command == "add") {
        add();
        }else if (command == "commit") {
        if (argc < 3) {
            std::cerr << "Commit message is missing.\n";
            return EXIT_FAILURE;
        }
        commit(argv[2]);
    } else if (command == "log") {
        show_commit_history();
    } else if (command == "branch") {
        if (argc < 3) {
            std::cerr << "Branch name is missing.\n";
            return EXIT_FAILURE;
        }
        create_branch(argv[2]);  
    } else if (command == "checkout") {
        if (argc < 3) {
            std::cerr << "Branch name is missing.\n";
            return EXIT_FAILURE;
        }
        switch_branch(argv[2]);
    } else if (command == "branches") {  
        show_branches();
    } else {
        std::cerr << "Unknown command " << command << '\n';
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}