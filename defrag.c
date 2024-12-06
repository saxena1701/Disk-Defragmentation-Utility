

#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <sys/stat.h>
// #include "disk_image.h"

#define OUTPUT_FILE_NAME "disk_defrag"
#define N_DBLOCKS 10 
#define N_IBLOCKS 4  
// /* Disk image created from input file */
// diskImage di;
// /* Pointer to defragmented disk image */
// diskImage *defragged = NULL;


  struct superblock {
    int blocksize; /* size of blocks in bytes */
    int inode_offset; /* offset of inode region in blocks */
    int data_offset; /* data region offset in blocks */
    int swap_offset; /* swap region offset in blocks */
    int free_inode; /* head of free inode list */
    int free_block; /* head of free block list */
  };


  
  struct inode {  
      int next_inode; /* list for free inodes */  
      int protect;        /*  protection field */ 
      int nlink;  /* Number of links to this file */ 
      int size;  /* Number of bytes in file */   
      int uid;   /* Owner's user ID */  
      int gid;   /* Owner's group ID */  
      int ctime;  /* Time field */  
      int mtime;  /* Time field */  
      int atime;  /* Time field */  
      int dblocks[N_DBLOCKS];   /* Pointers to data blocks */  
      int iblocks[N_IBLOCKS];   /* Pointers to indirect blocks */  
      int i2block;     /* Pointer to doubly indirect block */  
      int i3block;     /* Pointer to triply indirect block */  
   };

struct diskImage
{
    /* data */
    struct superblock *sb;
    struct inode *inodes;
    char *datablock;
    char *swapblocks;
    unsigned long totalsize;
    char* disk_start;

};

struct diskImage* write1024DiskImage(struct diskImage* src, struct diskImage* dest){
    char* file_buffer = (char*)malloc(src->totalsize);
    
    // for(int i=0;i<1024;i++){
    //     file_buffer[i] = src->disk_start[i];
    // }

    memcpy(file_buffer,src->disk_start,1024);

    int blocksize = src->sb->blocksize;
    struct superblock* sb = src->sb;
    int inode_offset = sb->inode_offset;
    int data_offset = sb->data_offset;
    int swap_offset = sb->swap_offset;

    dest->disk_start = file_buffer;
    dest->sb = (struct superblock*)(file_buffer + 512);
    dest->sb->inode_offset = inode_offset;
    dest->sb->data_offset = data_offset;
    dest->sb->swap_offset = swap_offset;

    dest->inodes = (struct inode*)(file_buffer + 1024 + inode_offset*blocksize);
    dest->datablock = (char*)(file_buffer + 1024 +data_offset*blocksize);
    dest->swapblocks = (char*)(file_buffer + 1024 +swap_offset*blocksize);
    dest->totalsize = src->totalsize;
    return dest;
}


void writeDisk(struct diskImage* dest,char* dest_file){

    char* file_buffer = dest->disk_start;
    int totalsize = dest->totalsize;

    FILE* new_file = fopen(dest_file,"wb");

    if(new_file==NULL){
        fprintf(stderr, "Error: Could not open file for writing");
        return;
    }

    int bytes = fwrite(file_buffer,sizeof(char),totalsize,new_file);

    if(bytes!=totalsize){
        printf("Not all bytes copied");
        return;
    }else{
        printf("bytes copied : %d\n",bytes);
    }

    fclose(new_file);
}


struct diskImage* readDiskImage(char* source_file){
    struct diskImage* di = (struct diskImage*)malloc(sizeof(struct diskImage));

    FILE *f = fopen(source_file,"rb");

    int filedesc = fileno(f);
    if(filedesc==-1){
        printf("file descriptor error");
        return NULL;
    }
    struct stat statfile;
    if(fstat(filedesc,&statfile)==-1){
        printf("error in getting disk image size");
        fclose(f);
        return NULL;
    }

    

    char* file_buffer = (char*)malloc(statfile.st_size);
    unsigned long file_size = statfile.st_size;
    unsigned long rsize = fread(file_buffer,1,file_size,f);

    if(rsize!=file_size){
        printf("error in reading the disk image");
        free(file_buffer);
        fclose(f);
        return NULL;
    }

    printf("file-size : %d\n",file_size);
    di->disk_start = file_buffer;
    di->totalsize = file_size;
    di->sb = (struct superblock*)(file_buffer + 512);
    struct superblock* sblock = di->sb;
    int blocksize = sblock->blocksize;

    printf("read blocksize : %d\n",blocksize);

    int inode_offset = sblock->inode_offset;
    di->inodes = (struct inode*)(file_buffer+1024+inode_offset*blocksize);
    int data_offset = sblock->data_offset;
    di->datablock = (char*)(file_buffer+1024+data_offset*blocksize);
    int swap_offset = sblock->swap_offset;
    di->swapblocks = (char*)(file_buffer+1024+swap_offset*blocksize);
    
    return di;

}


int main( int argc, char *argv[ ] )
{
    char* source_file = argv[1];
    char* dest_file = argv[2];

    struct diskImage* di = readDiskImage(source_file);
    struct diskImage* di_write = (struct diskImage*)malloc(sizeof(struct diskImage));

    di_write = write1024DiskImage(di,di_write);
    
    // char* write_inode_start = di_write->inodes;
    // char* write_datablock_start = di_write->datablock;
    char* temp = di->inodes;
    char* temp_inode = di_write->inodes;
    char* temp_data = di_write->datablock;
    int blocksize = (di->sb)->blocksize;

    while(temp<di->datablock){
        struct inode* point = (struct inode*)temp;
        if(point->nlink>0){
            //copy inode
            struct inode* new_inode = (struct inode*)temp_inode;
            *new_inode = *point;
            

            int file_size = point->size;
            
                //direct pointers case
            // printf("file_size : %d\n",point->size);
            // int* free_start = (int*)temp_data;
            for(int i=0;i<N_DBLOCKS;i++){
                if(point->dblocks[i]>0){
                    memcpy(temp_data, di->datablock + point->dblocks[i] * blocksize, blocksize);
                    new_inode->dblocks[i] = (temp_data-di_write->datablock)/blocksize;
                    temp_data+=blocksize;
                }
            }
            int block_pointers = blocksize/sizeof(int);
            
            
            for(int i=0;i<N_IBLOCKS;i++){
                if(point->iblocks[i]>0){
                    // printf("indirect datablock entry %d\n",point->iblocks[i]);
                    int* indirect_datablock = (int*)(di->datablock + point->iblocks[i]*blocksize);
                    int* free_data_region = (int*)temp_data;
                    temp_data+=blocksize;
                    for(int j=0;j<block_pointers;j++){
                        if(indirect_datablock[j]>0){
                            memcpy(temp_data, di->datablock + indirect_datablock[j] * blocksize, blocksize);
                            free_data_region[j] = (temp_data - di_write->datablock) / blocksize;
                            temp_data+=blocksize;
                        }
                    }
                    new_inode->iblocks[i] = (free_data_region - (int*)di_write->datablock)/blocksize;
                }
            }

            
            
            
            
            if(point->i2block>0){
                int* doubly_indirect_datablock = (int*)(di->datablock + (point->i2block)*blocksize);
                int* doubly_data_start = (int*)(temp_data);
                temp_data+=blocksize;
                for(int i=0;i<block_pointers;i++){
                if(doubly_indirect_datablock[i]>0){
                    int* indirect_datablock = (int*)(di->datablock + doubly_indirect_datablock[i]*blocksize);
                    int* free_data_start = (int*)temp_data;
                    temp_data+=blocksize;
                    for(int j=0;j<block_pointers;j++){
                        // printf("doubly value : %d\n",indirect_datablock[j]);
                        if(indirect_datablock[j]>0){
                            memcpy(temp_data, di->datablock + indirect_datablock[j] * blocksize, blocksize);
                            free_data_start[j] = (temp_data - di_write->datablock)/blocksize;
                            temp_data+=blocksize;
                        }
                    }
                    doubly_data_start[i] = (free_data_start - (int*)di_write->datablock)/blocksize;
                }
               }
               new_inode->i2block = (doubly_data_start - (int*)di_write->datablock)/blocksize;
               
            }

            


            if(point->i3block>0){
                temp_data+=blocksize;
                int* triply_indirect_datablock = (int*)(di->datablock+(point->i3block)*blocksize);
                int* triply_data_start = (int*)temp_data;
                temp_data+=blocksize;
                for(int i=0;i<block_pointers;i++){
                    if(triply_indirect_datablock[i]>0){
                        int* doubly_indirect_pointers = (int*)(di->datablock + triply_indirect_datablock[i]*blocksize);
                        int* doubly_data_start = (int*)temp_data;
                        temp_data+=blocksize;
                        for(int j=0;j<block_pointers;j++){
                            if(doubly_indirect_pointers[j]>0){
                                int* indirect_datablock = (int*)(di->datablock + doubly_indirect_pointers[j]*blocksize);
                                int* free_data_start = (int*)temp_data;
                                temp_data+=blocksize;
                                for(int k=0;k<block_pointers;k++){
                                    if(indirect_datablock[k]>0){
                                        memcpy(temp_data, di->datablock + indirect_datablock[k] * blocksize, blocksize);
                                        free_data_start[k] = (temp_data - di_write->datablock)/blocksize;
                                        temp_data+=blocksize;
                                    }
                                }
                                doubly_data_start[j] = (free_data_start - (int*)di_write->datablock)/blocksize;
                            }
                        }
                        triply_data_start[i] = (doubly_data_start - (int*)di_write->datablock)/blocksize;
                    }
                }

                new_inode->i3block = (triply_data_start - (int*)di_write->datablock)/blocksize;
                
            }
            
        }
        temp+=sizeof(struct inode);
        temp_inode += sizeof(struct inode);
    }
    
    
    writeDisk(di_write,dest_file);

    return 0;
}
