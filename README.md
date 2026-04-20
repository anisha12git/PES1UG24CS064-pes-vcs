# Building PES-VCS — A Version Control System from Scratch

**Objective:** Build a local version control system that tracks file changes, stores snapshots efficiently, and supports commit history. Every component maps directly to operating system and filesystem concepts.


## Student Details
- **Name:** Anisha Nukul Niranjan
- **SRN:** PES1UG24CS064
- **Section:** 4B 

---

# Phase 1: Object Storage Foundation

## Screenshots

### 1A
<img width="1600" height="284" alt="image" src="https://github.com/user-attachments/assets/59c2b711-f134-47d7-b8a2-094a3775d2c9" />

All Phase 1 tests passed, confirming correct blob storage, deduplication, and data integrity in the object store.

---

### 1B
<img width="1600" height="142" alt="image" src="https://github.com/user-attachments/assets/327b097d-5d9e-483b-a543-a66ab304a284" />


The object store contains hashed files organized in directories, showing how objects are stored and uniquely identified.

---

# Phase 2: Tree Objects

## Screenshots

### 2A
<img width="1192" height="228" alt="image" src="https://github.com/user-attachments/assets/a40f1e64-39e4-45eb-81fa-12f66be22963" />


All Phase 2 tests passed, confirming correct tree serialization, parsing, and deterministic structure handling.

---

### 2B
<img width="1600" height="225" alt="image" src="https://github.com/user-attachments/assets/d8ac35a2-9a2d-4c17-8419-10cb83911786" />

The object file content is stored in a serialized format with a header (blob) followed by the actual data, verifying correct object structure.

---

# Phase 3: The Index (Staging Area)

## Screenshots

### 3A
<img width="1302" height="180" alt="image" src="https://github.com/user-attachments/assets/0a4b35c1-cbc7-4c5e-b721-26fe2e8612c6" />

The status command correctly shows that both files have been successfully staged for commit.

---

### 3B
<img width="1600" height="162" alt="image" src="https://github.com/user-attachments/assets/21479a62-63f2-4ac9-a6d3-0b04fd572558" />

The index file correctly lists staged files with their metadata, confirming proper tracking in the staging area.

---

# Phase 4: Commits and History

## Screenshots

### 4A
<img width="1600" height="644" alt="image" src="https://github.com/user-attachments/assets/b307d156-2617-4231-b9e2-5ca867478ffb" />

The log command correctly displays the commit history with hashes, author details, timestamps, and messages.

---

### 4B
<img width="1600" height="835" alt="image" src="https://github.com/user-attachments/assets/de9d9414-e90b-4cea-94e3-411016ee8cf1" />

The increasing number of files in the object store shows how new commits create additional objects over time.

---

### 4C
<img width="1600" height="193" alt="image" src="https://github.com/user-attachments/assets/69273f73-9fe7-4565-a322-b0fdc30c640a" />

The HEAD file correctly points to the main branch, which in turn stores the latest commit hash, confirming proper reference linking.

---

### Final Observation
<img width="1600" height="484" alt="image" src="https://github.com/user-attachments/assets/72d40f12-c11e-4116-bad6-246cfdf9073f" />

The integration test initializes the repository successfully but fails during file staging due to a segmentation fault, indicating an issue in the `index_add` implementation.

---

# Phase 5 & 6: Analysis-Only Questions

### Q5.1
**Question:**  
A branch in Git is just a file in `.git/refs/heads/` containing a commit hash. Creating a branch is creating a file. Given this, how would you implement `pes checkout <branch>` — what files need to change in `.pes/`, and what must happen to the working directory? What makes this operation complex?

**Answer:**  
To implement `pes checkout <branch>`, the `HEAD` file must be updated to point to the selected branch reference. The working directory must be modified to match the tree of the target commit, including adding, updating, or deleting files. The index must also be updated accordingly. The complexity arises from handling differences between the current and target states and ensuring that local uncommitted changes are not lost.

---

### Q5.2
**Question:**  
When switching branches, the working directory must be updated to match the target branch's tree. If the user has uncommitted changes to a tracked file, and that file differs between branches, checkout must refuse. Describe how you would detect this "dirty working directory" conflict using only the index and the object store.

**Answer:**  
The working directory is compared with the index by recomputing file hashes and matching them with stored hashes. If differences are found, the file is considered modified. These modified files are then compared with the target branch’s version from the object store. If a file differs in both places, a conflict is detected and checkout is aborted to prevent data loss.

---

### Q5.3
**Question:**  
"Detached HEAD" means HEAD contains a commit hash directly instead of a branch reference. What happens if you make commits in this state? How could a user recover those commits?

**Answer:**  
Commits made in a detached HEAD state are not linked to any branch and can become unreachable. If no reference points to them, they may be deleted during garbage collection. A user can recover these commits by creating a new branch pointing to the current commit, thereby preserving them.

---

### Q6.1
**Question:**  
Over time, the object store accumulates unreachable objects — blobs, trees, or commits that no branch points to (directly or transitively). Describe an algorithm to find and delete these objects. What data structure would you use to track "reachable" hashes efficiently? For a repository with 100,000 commits and 50 branches, estimate how many objects you'd need to visit.

**Answer:**  
Start from all branch references and traverse all reachable commits, trees, and blobs recursively. Store visited object hashes in a hash set for efficient lookup. After traversal, iterate through all objects in the object store and delete those not present in the reachable set. For a repository with 100,000 commits and 50 branches, the traversal would visit approximately all reachable objects, which may be on the order of hundreds of thousands depending on file history.

---

### Q6.2
**Question:**  
Why is it dangerous to run garbage collection concurrently with a commit operation? Describe a race condition where GC could delete an object that a concurrent commit is about to reference. How does Git's real GC avoid this?

**Answer:**  
During a commit, new objects may be created but not yet referenced by any branch. If garbage collection runs at the same time, it may consider these objects unreachable and delete them. Git avoids this by using atomic updates, temporary object storage, and delaying deletion of unreachable objects, ensuring that newly created objects are not removed prematurely.
