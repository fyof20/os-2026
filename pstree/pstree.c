#include <stdio.h>
#include <stdlib.h>
#include <dirent.h>
#include <ctype.h>
#include <string.h>
#include <unistd.h>
#include <getopt.h>
#include <stdbool.h>

#define MAX_PROCESSES 1024

//定义进程
typedef struct process {
    pid_t pid;
    pid_t ppid;
    char name[256];
};

// 读取进程名
// @param
// pid 进程号
// buf 缓冲区
// n 缓冲区大小
static int read_comm(pid_t pid, char *buf, size_t n) {
    char path[64];
    snprintf(path, sizeof(path), "/proc/%d/comm", pid);
    FILE *f = fopen(path, "r");
    if (!f) return -1;
    if (!fgets(buf, (int)n, f)) { fclose(f); return -1; }
    buf[strcspn(buf, "\n")] = 0;
    fclose(f);
    return 0;
}

static int get_ppid_from_stat(pid_t pid, pid_t *ppid_out) {
    /**
     * 
     */
    char path[64], line[4096];
    snprintf(path, sizeof(path), "/proc/%d/stat", pid);
    FILE *f = fopen(path, "r");
    if (!f) return -1;
    if (!fgets(line, sizeof(line), f)) { fclose(f); return -1; }
    fclose(f);

    int id, ppid;
    char comm[256], state;
    if (sscanf(line, "%d (%255[^)]) %c %d", &id, comm, &state, &ppid) != 4) return -1;
    *ppid_out = (pid_t)ppid;
    return 0;
}
//遍历/proc
static int collect_process(struct process procs[], int *count){
    DIR *d = opendir("/proc");
    if(!d){
        perror("打开/proc失败");
        return EXIT_FAILURE;
    }
    int n = 0;
    struct dirent *de;
    while ((de = readdir(d)) != NULL && n<MAX_PROCESSES){
        if(!isdigit((unsigned char)de->d_name[0])){
            continue;
        }
        pid_t id =(pid_t) atoi(de->d_name);
        procs[n].pid=id;
        if(get_ppid_from_stat(id, &procs[n].ppid) != 0){
            //读取失败"没有父进程"
            procs[n].ppid = 0;
        }
        read_comm(id, procs[n].name, 256);
        n++;
    }
    closedir(d);
    *count = n;
    return 0;
}

static int find_root(struct process procs[], int count){
    for(int i=0;i<count;i++){
        if(procs[i].pid == 1){
            return i;
        }
    }
    return 0;
}

static int find_children(struct process procs[], int count, pid_t ppid, pid_t children[], int *child_count) {
    *child_count = 0;
    for (int i = 0; i < count; i++) {
        if (procs[i].ppid == ppid) {
            children[*child_count] = procs[i].pid;
            (*child_count)++;
        }
    }
    return 0;
}
static int cmp_pid(const void *a, const void *b){
    pid_t pa = *(pid_t*)a;
    pid_t pb = *(pid_t*)b;
    return (pa > pb) - (pa < pb);
}

static void print_tree(struct process procs[], int count, pid_t pid, bool numeric_sort, bool show_pid, int depth){
    //find current proc
    int idx = -1;
    for(int i=0;i<count;i++){
        if(procs[i].pid == pid){
            idx = i;
            break;
        }
    }
    if(idx == -1) return;

    for(int i=0;i<depth;i++){
        printf(" ");
    }
    //print
    if(show_pid){
        printf("|- %s(%d)\n", procs[idx].name, pid);
    }else{
        printf("|- %s\n",procs[idx].name);
    }
    //find child proc
    pid_t children[MAX_PROCESSES];
    int child_count = 0;
    find_children(procs, count, pid, children, &child_count);

    if (numeric_sort && child_count > 1) {
        qsort(children, child_count, sizeof(pid_t), cmp_pid);
    }

    for (int i = 0; i < child_count; i++) {
        print_tree(procs, count, children[i], numeric_sort, show_pid, depth + 1);
    }


}

int main(int argc, char *argv[]){

    bool show_pid = false;
    bool numeric_sort = false;
    bool version = false;

    static struct option long_options[] = {
        {"show-pids", no_argument, 0, 'p'},
        {"numeric-sort", no_argument, 0, 'n'},
        {"version", no_argument, 0, 'V'},
        {0, 0, 0, 0},
    };
    int opt;
    int option_index = 0;
    while((opt = getopt_long(argc, argv, "pnV", long_options, &option_index)) != -1){
        switch (opt) {
            case 'p':
                show_pid = true;
                break;
            case 'V':
                version = true;
                break;
            case 'n':
                numeric_sort = true;
                break;
            default:
                perror("invlid option\n");
        }
    }
    if(version){
        printf("my pstree(2026)\n");
        return 0;
    }

    struct process procs[MAX_PROCESSES];
    int count = 0;
    collect_process(procs, &count);

    // 找根进程 (pid=1)
    int root_idx = find_root(procs, count);

    // 打印树
    print_tree(procs, count, procs[root_idx].pid, numeric_sort, show_pid, 0);

    return 0;

}



/** 
int main(void) {
    pid_t self = getpid();
    pid_t parent = getppid();

    char self_comm[256] = "?", parent_comm[256] = "?";
    read_comm(self, self_comm, sizeof self_comm);
    read_comm(parent, parent_comm, sizeof parent_comm);

    printf("%s(%d)\n", parent_comm, parent);

    DIR *d = opendir("/proc");
    if (!d) { perror("opendir /proc"); return 1; }

    struct dirent *de;
    while ((de = readdir(d)) != NULL) {
        if (!isdigit((unsigned char)de->d_name[0])) continue;
        pid_t pid = (pid_t)atoi(de->d_name);

        pid_t ppid;
        if (get_ppid_from_stat(pid, &ppid) != 0) continue;
        if (ppid != parent) continue;

        char comm[256] = "?";
        read_comm(pid, comm, sizeof comm);

        printf("  |- %s(%d)%s\n", comm, pid, (pid == self) ? "  <== me" : "");
    }

    closedir(d);
    return 0;
}
*/