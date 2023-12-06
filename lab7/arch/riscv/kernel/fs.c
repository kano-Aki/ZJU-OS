#include "fs.h"
#include "buf.h"
#include "defs.h"
#include "slub.h"
#include "task_manager.h"
#include "virtio.h"
#include "vm.h"
#include "mm.h"

// --------------------------------------------------
// ----------- read and write interface -------------

void disk_op(int blockno, uint8_t *data, bool write) {
    struct buf b;
    b.disk = 0;
    b.blockno = blockno;
    b.data = (uint8_t *)PHYSICAL_ADDR(data);
    virtio_disk_rw((struct buf *)(PHYSICAL_ADDR(&b)), write);
}

#define disk_read(blockno, data) disk_op((blockno), (data), 0)
#define disk_write(blockno, data) disk_op((blockno), (data), 1)

// -------------------------------------------------
// 全局变量
struct sfs_fs SFS;
bool HaveInit = 0;//保存是否初始化过
void strcpy(char *a, const char *b) {
  while (*b) {
    *a++ = *b++;
  }
  *a = '\0';
}
int strlen(const char *a){
    int len = 0;
    while(a[len]) len++;
    return len;
}
#define min(X, Y)  ((X) < (Y) ? (X) : (Y))
#define max(X, Y)  ((X) < (Y) ? (Y) : (X))
// ------------------ your code --------------------

int sfs_init(){//应该直接读取块中的内容比较好
    disk_read(0,(&SFS.super));//读取内容
    SFS.freemap = (struct bitmap *) kmalloc(sizeof(struct bitmap));
    if(SFS.freemap == NULL)
        return -1;
    disk_read(2, SFS.freemap->freemap);//读取freemap
    SFS.super_dirty = 0; //刚开始并没有修改过超级块或者freemap
    memset(SFS.block_list, NULL, sizeof(SFS.block_list));
    HaveInit = 1;
    return 0;
}

#define Hash(x) ((x) & 1023)

//获取一个block
void* BufferRead(uint32_t blockno, bool ifpin){
    //先在内存池中查找，如果有的话优先返回内容
    // printf("Pin %d\n", blockno);
    int Hashid = Hash(blockno);
    struct ListNode* node = NULL;
    for(node = SFS.block_list[Hashid]; node != NULL; node = node->next)
        if(blockno == node->data->blockno)
            break;
    //如果找到就直接返回
    if(node != NULL){
        node->data->pin_count += ifpin;
        return node->data->block;
    }
    //初始化新块
    struct ListNode* newnode = kmalloc(sizeof(struct ListNode));
    newnode->data = kmalloc(sizeof(struct sfs_memory_block));
    struct sfs_memory_block* data = newnode->data;
    data->block = kmalloc(4096);
    data->blockno = blockno;
    data->dirty = 0;
    data->pin_count = ifpin;
    disk_read(blockno, data->block);
    //没有的话需要插入这个块
    if(SFS.block_list[Hashid] == NULL){//第一个块为空就直接替换
        SFS.block_list[Hashid] = newnode;
        newnode->next = NULL;
        newnode->prev = NULL;
    }
    else{
        struct ListNode* Second = SFS.block_list[Hashid]->next;
        SFS.block_list[Hashid]->next = newnode;
        newnode->prev = SFS.block_list[Hashid];
        newnode->next = Second;
        if(Second != NULL)
            Second->prev = newnode;
    }
    return data->block;
}
//解锁一个块
void BufferUnpin(uint32_t blockno){
    // printf("Unpin %d\n", blockno);
    int Hashid = Hash(blockno);
    struct ListNode* node = NULL;
    for(node = SFS.block_list[Hashid]; node != NULL; node = node->next)
        if(blockno == node->data->blockno)
            break;
    //如果找到就直接返回
    if(node != NULL){
        node->data->pin_count--;
        // printf("After Unpin %d, pin_count is %d\n", blockno, node->data->pin_count);
        return;
    }
    puts("BufferUnpin Fail");
}
//设置dirty
void BufferSetdirty(uint32_t blockno){
    int Hashid = Hash(blockno);
    struct ListNode* node = NULL;
    for(node = SFS.block_list[Hashid]; node != NULL; node = node->next)
        if(blockno == node->data->blockno)
            break;
    //如果找到就直接返回
    if(node != NULL){
        node->data->dirty = 1;
        return;
    }
    puts("BufferSetdirty Fail");
}
//尝试写回某个块
void BufferWriteBack(uint32_t blockno){
    printf("Write Back %d\n", blockno);
    int Hashid = Hash(blockno);
    struct ListNode* node = NULL;
    for(node = SFS.block_list[Hashid]; node != NULL; node = node->next)
        if(blockno == node->data->blockno)
            break;
    //如果找不到就直接返回
    if(node == NULL)
        return;
    if(node->data->pin_count < 0){//FOR DEBUG
        printf("Error pincount: %d\n", blockno);
        exit(0);
    }
    if(node->data->dirty)//脏块写回
        disk_write(blockno, node->data->block);
    if(node->data->pin_count == 0){//如果没有指针引用就可以释放
        //释放空间
        kfree(node->data->block);
        kfree(node->data);
        struct ListNode* nextnode = node->next;
        if(node == SFS.block_list[Hashid]){//删除链表头节点
            SFS.block_list[Hashid] = nextnode;
            if(nextnode != NULL)
                nextnode->prev = NULL;
        }
        else{
            struct ListNode* prevnode = node->prev;//prev一定存在
            prevnode->next = nextnode;
            if(nextnode != NULL)
                nextnode->prev = prevnode;
        }
        kfree(node);
    }
}
//找到一个新的空Block的编号
int NewBlock(){//暂时不考虑磁盘空间不够的情况
    SFS.super.unused_blocks--;
    SFS.super_dirty = 1;
    int i = 0, j = 0;
    for(;i <= 4096 && SFS.freemap->freemap[i] == 0xFF; i++);//找到第一个可能空的块
    for(;(SFS.freemap->freemap[i] >> j) & 1; j++);//找到空块里的第一个空点
    SFS.freemap->freemap[i] |= 1 << j;//初始化
    return (i << 3) + j;
}
int sfs_open(const char *path, uint32_t flags){
    printf("sfs_get_files path: %s\n", path);
    if(!HaveInit)
        sfs_init();
    if(path[0] != '/')
        return -1;//说明路径有问题
    struct sfs_inode* NowNode = (struct sfs_inode*)BufferRead(1, 1), *PreNode = NULL, *NextNode = NULL;//PreNode存储上一个节点
    int index = 1, next_slash = 1;
    uint32_t nowino = 1, preino = 1, nextino = 1;
    while(path[next_slash] != 0){//只要没到路径的结尾就继续查找
        next_slash = index;
        while(path[next_slash] && path[next_slash] != '/')
            next_slash++;//找到下一个'/'的位置
        printf("%d %d %d\n", nowino, next_slash, index);
        if(NowNode->type == SFS_FILE)//如果现在已经是文件
            return -1;//目前的路径不对
        else{//如果是目录
            bool ifok = 0;
            int nentry = NowNode->size / sizeof(struct sfs_entry);//entry的总数量
            printf("nentry: %d\n", nentry);
            //直接索引
            for(int i = 0; i < NowNode->blocks && i < SFS_NDIRECT; i++){
                struct sfs_entry* entry = BufferRead(NowNode->direct[i], 1);
                printf("NowNode->direct[i]: %d\n", NowNode->direct[i]);
                for(int j = 0; j < SFS_NENTRY && i * SFS_NENTRY + j < nentry; j++){
                    bool flag = 1;
                    // /   a  b c d e f g h   /
                    //  index             next_slash
                    for(int k = 0; k < strlen(entry[j].filename); k++)
                        putchar(entry[j].filename[k]);
                    putchar(' ');
                    flag &= ((next_slash - index) == strlen(entry[j].filename));
                    if(!flag) continue;
                    for(int k = 0; k < next_slash - index; k++)
                        if(entry[j].filename[k] != path[index + k]){
                            flag = 0;
                            break;
                        }
                    if(flag){//找到了一个存在的目录
                        nextino = entry[j].ino;
                        BufferUnpin(NowNode->direct[i]);
                        NextNode = (struct sfs_inode*)BufferRead(entry[j].ino, 1);
                        ifok = 1;
                        break;
                    }
                }
                if(ifok)//如果已经找到就结束
                    break;
                BufferUnpin(NowNode->direct[i]);
            }
            //间接索引TODO，一般用不到这种情况
            printf("\n%s\n", ifok == 0 ? "not find" : "find");
            if(!ifok){//找不到就要看权限
                if(flags & SFS_FLAG_WRITE == 0)
                    return -2;//路径不存在,且权限不够
                int newblockno = NewBlock();//新块的inode
                //初始化新块
                struct sfs_inode* NewNode = (struct sfs_inode*)BufferRead(newblockno, 1);
                NewNode->size      = path[next_slash] ? sizeof(struct sfs_entry) << 1 : 0;//目录要有两个entry，文件没有大小
                NewNode->type      = path[next_slash] ? SFS_DIRECTORY : SFS_FILE;//type需要看是不是路径的末尾
                NewNode->links     = 1;
                NewNode->blocks    = 1;
                NewNode->direct[0] = NewBlock();//申请一个新的块
                printf("%d %d\n", newblockno, NewNode->direct[0]);
                NewNode->indirect  = 0;
                BufferSetdirty(newblockno);//设置成脏块
                struct sfs_entry Newentry[2]; 
                if(NewNode->type == SFS_DIRECTORY){
                    Newentry[0].ino = newblockno;
                    strcpy(Newentry[0].filename, ".");
                    Newentry[1].ino = nowino;
                    strcpy(Newentry[1].filename, "..");
                    disk_write(NewNode->direct[0], &Newentry);//写回新块，不用读到BufferPool里
                }
                //将这块给它接到父亲节点上
                struct sfs_entry newentry;
                for(int k = 0; k < next_slash - index; k++)
                    newentry.filename[k] = path[index + k];
                newentry.filename[next_slash - index] = 0;
                newentry.ino = newblockno;
                if(NowNode->size != NowNode->blocks * sizeof(struct sfs_entry) * SFS_NENTRY){//即最后一块不是满的
                    struct sfs_entry* LastBlock = BufferRead(NowNode->direct[NowNode->blocks - 1], 1);//取出最后一块    
                    LastBlock[nentry % SFS_NENTRY] = newentry;//这里我们用指针操作，已经修改了BufferPool中的内容
                    BufferSetdirty(NowNode->direct[NowNode->blocks - 1]);
                    BufferUnpin(NowNode->direct[NowNode->blocks - 1]);
                    //间接索引TODO
                }
                else{//最后一块是满的，那就申请一个新块
                    NowNode->direct[NowNode->blocks] = NewBlock();
                    disk_write(NowNode->direct[NowNode->blocks], &newentry);//写回新块，不用读到BufferPool里
                    NowNode->blocks++;
                    //间接索引TODO
                }
                NowNode->size += sizeof(struct sfs_entry);//多了一个节点
                BufferSetdirty(nowino);//这个块被我们修改了
                NextNode = NewNode;
                nextino = newblockno;
            }
        }
        //这里可以释放掉老的PreNode
        if(!(preino == nowino && preino == 1))//第一次不能释放
            BufferUnpin(preino);
        PreNode = NowNode;
        preino = nowino;
        NowNode = NextNode;
        nowino = nextino;
        index = next_slash + 1;
        printf("nowino: %d preino: %d\n\n\n", nowino, preino);
    }
    if(NowNode->type == SFS_DIRECTORY)
        return -1;//说明有同名的目录干扰
    //创建文件，找一个空的指针填入
    puts("end of open\n");
    for(int i = 0; i < 16; i++)
        if(current->fs.fds[i] == NULL){
            current->fs.fds[i] = kmalloc(sizeof(struct file));
            current->fs.fds[i]->flags = flags;
            current->fs.fds[i]->inode = NowNode;
            current->fs.fds[i]->off = 0;//刚打开还没有offset
            current->fs.fds[i]->path = PreNode;//PreNode和NowNode我们会在文件close的时候
            current->fs.fds[i]->ino = nowino;
            current->fs.fds[i]->fa_ino = preino;
            return i;
        }
    return -3;//打开了太多的文件
}
//将一个inode的所有数据块写回
void WirteBackDataBlocks(struct sfs_inode * nownode, bool Type){
    uint32_t sizePerBlock = (Type == SFS_DIRECTORY ? SFS_NENTRY * sizeof(struct sfs_entry) : 4096); 
    printf("nownode->size: %d\n", nownode->size);
    for(int i = 0; i < SFS_NDIRECT && i < (nownode->size + sizePerBlock - 1) / sizePerBlock; i++){
        // BufferUnpin(nownode->direct[i]);
        printf("%d ", nownode->direct[i]);
        BufferWriteBack(nownode->direct[i]);
    }
    puts("\n");
    if((nownode->size >> 12) > SFS_NDIRECT){//文件过大需要考虑间接索引是否存在
        uint32_t* indirect = BufferRead(nownode->indirect, 1);
        for(int i = SFS_NDIRECT; i < (nownode->size + sizePerBlock - 1) / sizePerBlock; i++){
            printf("i: %d\n", i);
            // BufferUnpin(indirect[i - SFS_NDIRECT]);
            BufferWriteBack(indirect[i - SFS_NDIRECT]);
        }
        BufferUnpin(nownode->indirect);
        BufferWriteBack(nownode->indirect);
    }
}
//关闭文件的时候写回文件以及到根目录上的所有目录文件，以及它们的数据块
int sfs_close(int fd){
    struct file* f = current->fs.fds[fd];
    if(f == NULL)
        return -1;//根本不存在
    //将文件写回
    puts("sfs_close: WriteBack files\n");
    struct sfs_inode * nownode = f->inode;
    WirteBackDataBlocks(nownode, SFS_FILE);
    puts("sfs_close: WriteBack fileinode\n");
    BufferUnpin(f->ino);
    BufferWriteBack(f->ino);
    //考虑将路径上所有访问过的点及其数据块写回，否则我们无法将BufferPool同步回磁盘中
    puts("sfs_close: WriteBack dirs\n");
    nownode = f->path;
    uint32_t nowino = f->fa_ino;
    while(nowino != 1){//1说明是到达了/，就不能再进一步回退了
        printf("sfs_close: nowino %d\n", nowino);
        struct sfs_entry* entrys = BufferRead(nownode->direct[0], 1);
        uint32_t nextino = entrys[1].ino;//..上层目录文件
        BufferUnpin(nownode->direct[0]);
        WirteBackDataBlocks(nownode, SFS_DIRECTORY);
        if(nowino == f->fa_ino)
            BufferUnpin(nowino);//f->ino和f->fa_ino之前open没有unpin所以现在unpin
        BufferWriteBack(nowino);
        nowino = nextino;
        nownode = BufferRead(nowino, 1);
    }
    //写回根目录索引
    puts("sfs_close: WriteBack RootInode\n");
    printf("sfs_close: nowino %d\n", nowino);
    WirteBackDataBlocks(nownode, SFS_DIRECTORY);
    BufferWriteBack(nowino);
    //写回bitmap和super块
    if(SFS.super_dirty){
        disk_write(0, &SFS.super);
        disk_write(2, SFS.freemap->freemap);
        SFS.super_dirty = 0;
    }
    //释放空间
    kfree(current->fs.fds[fd]);
    current->fs.fds[fd] = NULL;
    return 0;
}

int sfs_seek(int fd, int32_t off, int fromwhere){
    struct file * f = current->fs.fds[fd];
    switch (fromwhere)
    {
        case SEEK_SET://文件头
            f->off = off;
            break;
        case SEEK_END://文件末尾
            f->off = f->inode->size - off;
            break;
        default://SEEK_CUR(当前)
            f->off = f->off + off;
            break;
    }
    if(f->off < 0 || f->off >= f->inode->size)//超出边界
        return -1;//返回越界
    else
        return 0;//正常返回
}

int sfs_read(int fd, char *buf, uint32_t len){
    struct file * f = current->fs.fds[fd];
    len = min(len, f->inode->size - f->off);//这一条能够保证后面不需要再判断越界的情况
    int blockindex = f->off / 4096;
    int off = f->off % 4096;
    uint32_t bufoff = 0;
    struct sfs_inode* nownode = f->inode;
    //先搜索直接索引
    while(blockindex < SFS_NDIRECT && len){
        uint32_t blocklen = min(len, 4096 - off);
        char* block = BufferRead(nownode->direct[blockindex], 1);
        memcpy(buf + bufoff, block + off, blocklen);
        BufferUnpin(nownode->direct[blockindex]);
        bufoff += blocklen;
        len -= blocklen;
        blockindex++;
        off = 0;
    }
    //接下来间接索引,不考虑文件过大连间接索引都无法承载的情况
    if(len){
        blockindex -= SFS_NDIRECT;
        uint32_t* indirect = BufferRead(nownode->indirect, 1);
        while(len){
            uint32_t blocklen = min(len, 4096 - off);
            char* block = BufferRead(indirect[blockindex], 1);
            memcpy(buf + bufoff, block + off, blocklen);
            BufferUnpin(indirect[blockindex]);
            bufoff += blocklen;
            len -= blocklen;
            blockindex++;
            off = 0;
        }
        BufferUnpin(nownode->indirect);
    }
    f->off += bufoff;
    return bufoff;
}

int sfs_write(int fd, char *buf, uint32_t len){
    struct file * f = current->fs.fds[fd];
    if(f->flags & SFS_FLAG_WRITE == 0){
        return -5;//权限错误
    }
    int blockindex = f->off / 4096;
    int off = f->off % 4096;
    // printf("blockindex: %d off: %d\n", blockindex, off);
    uint32_t bufoff = 0;
    //更新size
    if(len > f->inode->size - f->off){//超出了大小
        f->inode->size = f->off + len;//更新size
        BufferSetdirty(f->ino);
    }
    struct sfs_inode* nownode = f->inode;
    // printf(".ino: %d\n", f->ino);
    //先搜索直接索引
    while(blockindex < SFS_NDIRECT && len){
        if(blockindex >= nownode->blocks){//块超出范围
            nownode->direct[blockindex] = NewBlock();
            nownode->blocks++;
        }
        uint32_t blocklen = min(len, 4096 - off);
        char* block = BufferRead(nownode->direct[blockindex], 1);
        memcpy(block + off, buf + bufoff, blocklen);
        BufferSetdirty(nownode->direct[blockindex]);
        BufferUnpin(nownode->direct[blockindex]);
        bufoff += blocklen;
        len -= blocklen;
        blockindex++;
        off = 0;
    }
    //接下来间接索引,不考虑文件过大连间接索引都无法承载的情况
    if(len){
        bool firstdirty = 0;
        blockindex -= SFS_NDIRECT;
        if(nownode->indirect == 0){//如果没有indirect的节点还需要先申请
            nownode->indirect = NewBlock();
        }
        uint32_t* indirect = BufferRead(nownode->indirect, 1);
        while(len){
            uint32_t blocklen = min(len, 4096 - off);
            if(blockindex + SFS_NDIRECT >= nownode->blocks){
                indirect[blockindex] = NewBlock();
                if(!firstdirty){
                    firstdirty = 1;
                    BufferSetdirty(nownode->indirect);
                }
                nownode->blocks++;
            }
            char* block = BufferRead(indirect[blockindex], 1);
            memcpy(block + off, buf + bufoff, blocklen);
            BufferSetdirty(indirect[blockindex]);
            BufferUnpin(indirect[blockindex]);
            bufoff += blocklen;
            len -= blocklen;
            blockindex++;
            off = 0;
        }
        BufferUnpin(nownode->indirect);
    }
    f->off += bufoff;
    return bufoff;
}

int sfs_get_files(const char* path, char* files[]){
    printf("sfs_get_files path: %s\n", path);
    if(!HaveInit)
        sfs_init();
    if(path[0] != '/')
        return -5;//说明路径有问题
    struct sfs_inode* NowNode = (struct sfs_inode*)BufferRead(1, 1), *PreNode = NULL, *NextNode = NULL;//PreNode存储上一个节点
    int index = 1, next_slash = 1;
    uint32_t nowino = 1, preino = 1, nextino = 1;
    while(path[next_slash] != 0){//只要没到路径的结尾就继续查找
        next_slash = index;
        while(path[next_slash] && path[next_slash] != '/')
            next_slash++;//找到下一个'/'的位置
        if(next_slash == index){// 
            if(path[index] == 0 && path[index] == '/')//合法情况，如/home/
                break;
            else
                return -1;// //连用，无法识别
        }
        if(NowNode->type == SFS_FILE)//如果现在已经是文件
            return -1;//目前的路径不对
        else{//如果是目录
            bool ifok = 0;
            int nentry = NowNode->size / sizeof(struct sfs_entry);//entry的总数量
            // printf("nentry: %d\n", nentry);
            //直接索引
            for(int i = 0; i < NowNode->blocks && i < SFS_NDIRECT; i++){
                struct sfs_entry* entry = BufferRead(NowNode->direct[i], 1);
                // printf("NowNode->direct[i]: %d\n", NowNode->direct[i]);
                for(int j = 0; j < SFS_NENTRY && i * SFS_NENTRY + j < nentry; j++){
                    bool flag = 1;
                    // /   a  b c d e f g h   /
                    //  index             next_slash
                    // for(int k = 0; k < strlen(entry[j].filename); k++)
                    //     putchar(entry[j].filename[k]);
                    // putchar(' ');
                    flag &= ((next_slash - index) == strlen(entry[j].filename));
                    if(!flag) continue;
                    for(int k = 0; k < next_slash - index; k++)
                        if(entry[j].filename[k] != path[index + k]){
                            flag = 0;
                            break;
                        }
                    if(flag){//找到了一个存在的目录
                        nextino = entry[j].ino;
                        BufferUnpin(NowNode->direct[i]);
                        NextNode = (struct sfs_inode*)BufferRead(entry[j].ino, 1);
                        ifok = 1;
                        break;
                    }
                }
                if(ifok)//如果已经找到就结束
                    break;
                BufferUnpin(NowNode->direct[i]);
            }
            //间接索引TODO，一般用不到这种情况
            // printf("\n%s\n", ifok == 0 ? "not find" : "find");
            if(!ifok){//找不到就直接结束
                return -1;
            }
        }
        //这里可以释放掉老的PreNode
        if(!(preino == nowino && preino == 1))//第一次不能释放
            BufferUnpin(preino);
        PreNode = NowNode;
        preino = nowino;
        NowNode = NextNode;
        nowino = nextino;
        index = next_slash + 1;
        // printf("nowino: %d preino: %d\n\n\n", nowino, preino);
    }
    if(NowNode->type == SFS_FILE)
        return 0;//当前目录是一个文件
    //这里我们认为实现的是ls命令，即文件名包括.和..和目录
    uint32_t sizePerBlock = SFS_NENTRY * sizeof(struct sfs_entry), tot = 0;
    for(int i = 0; i < SFS_NDIRECT && i < (NowNode->size + sizePerBlock - 1) / sizePerBlock; i++){
        struct sfs_entry* entrys = BufferRead(NowNode->direct[i], 1);
        for(int j = 0; j < SFS_NENTRY && i * sizePerBlock + j * sizeof(struct sfs_entry) < NowNode->size; j++)
            strcpy(files[tot++], entrys[j].filename);
        BufferUnpin(NowNode->direct[i]);
    }
    if((NowNode->size >> 12) > SFS_NDIRECT){//文件过大需要考虑间接索引是否存在
        uint32_t* indirect = BufferRead(NowNode->indirect, 1);
        for(int i = SFS_NDIRECT; i < (NowNode->size + sizePerBlock - 1) / sizePerBlock; i++){
            struct sfs_entry* entrys = BufferRead(indirect[i - SFS_NDIRECT], 1);
            for(int j = 0; j < SFS_NENTRY && i * sizePerBlock + j * sizeof(struct sfs_entry) < NowNode->size; j++)
                strcpy(files[tot++], entrys[j].filename);
            BufferUnpin(indirect[i - SFS_NDIRECT]);
        }
        BufferUnpin(NowNode->indirect);
    }
    return tot;
}