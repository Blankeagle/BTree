// # @File:  bplustree.cc
// # @Brief
// # BTreeplus的实现类
// # @Author Jicheng Zhu
// # @Email 2364464666@qq.com
// #
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "../include/bplustree.h"
enum
{
    BPLUS_TREE_LEAF,
    BPLUS_TREE_NON_LEAF = 1,
};
enum
{
    LEFT_SIBLING,
    RIGHT_SIBING = 1,
};

static inline int is_leaf(struct bplus_node *node)
{
    return node->type == BPLUS_TREE_LEAF;
}

static struct bplus_non_leaf *non_leaf_new()
{
    /* data */
    struct bplus_non_leaf *node = (bplus_non_leaf *)calloc(1, sizeof(*node));
    assert(node != NULL);
    list_init(&node->link);
    node->type = BPLUS_TREE_NON_LEAF;
    node->parent_key_idx = -1;
    return node;
};

static struct bplus_leaf *leaf_new()
{
    struct bplus_leaf *node = (bplus_leaf *)malloc(sizeof(*node));
    assert(node != NULL);
    list_init(&node->link);
    node->type = BPLUS_TREE_LEAF;
    node->parent_key_idx = -1;
    return node;
}

static key_t key_binary_search(key_t *arr, int len, key_t target) //二分法搜索
{
    int low = -1;
    int high = len;
    while (low + 1 < high)
    {
        int mid = low + (high + low) / 2;
        if (target > arr[mid])
        {
            low = mid;
        }
        else
        {
            high = mid;
        }
    }
    if (high >= len || arr[high] != target) //在本key下
    {
        return -high - 1;
    }
    else
    {
        return high;
    }
}

static int bplus_tree_search(bplustree *tree, key_t key) //查找数据
{
    int i, ret = -1;
    struct bplus_node *node = tree->root;
    while (node != NULL)
    {
        if (is_leaf(node)) //找到叶子节点
        {
            bplus_leaf *ln = (bplus_leaf *)node;
            i = key_binary_search(ln->key, ln->entries, key);
            ret = (i >= 0 ? ln->data[i] : 0);
            break;
        }
        else
        {
            bplus_non_leaf *nln = (bplus_non_leaf *)node;
            i = key_binary_search(nln->key, nln->children - 1, key); //key的长度比指针长度小1
            if (i > 0)                                               //在下一节点处
            {
                node = nln->sub_ptr[i + 1];
            }
            else
            {
                i = (-i - 1);
                node = nln->sub_ptr[i];
            }
        }
    }
    return ret;
}
int bplustree::bplus_tree_get(key_t key) //获取节点
{

    int ret = bplus_tree_search(this, key);
    if (ret)
    {
        return ret;
    }
    else
        return -1;
}

static void leaf_split_left(bplus_leaf *leaf, bplus_leaf *left, key_t key, int data, int insert) //将leaf左边的节点复制到left，并将leaf的数据左移
{
    int i, j;
    int split = (leaf->entries + 1) / 2;
    __list_add(&left->link, leaf->link.prev, &leaf->link); //TOOD 位置错了
    //复制数据从0到key[split-2]
    for (i = 0, j = 0; i < split - 1; j++) //i控制leaf数组，j控制left
    {
        if (j == insert)
        {
            left->key[j] = key;
            left->data[j] = data;
        }
        else
        {
            left->key[j] = leaf->key[i];
            left->data[j] = leaf->data[i];
            i++;
        }
    }
    if (j == insert)
    {
        left->key[j] = key;
        left->data[j] = data;
        j++;
    }
    left->entries = j;
    //将leaf数据左移
    for (j = 0; j < leaf->entries; i++, j++) //i初值为分割的位置
    {
        leaf->key[j] = leaf->key[i];
        leaf->data[j] = leaf->data[i];
    }
    leaf->entries = j;
}

static void leaf_split_right(bplus_leaf *leaf, bplus_leaf *right, key_t key, int data, int insert)
{

    int i, j;
    /* split = [m/2] */
    int split = (leaf->entries + 1) / 2;
    /* split as right sibling */
    //list_add(&right->link, &leaf->link);
    __list_add(&right->link, &leaf->link, leaf->link.next);
    /* replicate from key[split] */
    for (i = split, j = 0; i < leaf->entries; j++)
    {
        if (j != insert - split)
        {
            right->key[j] = leaf->key[i];
            right->data[j] = leaf->data[i];
            i++;
        }
    }
    /* reserve a hole for insertion */
    if (j > insert - split)
    {
        right->entries = j;
    }
    else
    {
        assert(j == insert - split);
        right->entries = j + 1;
    }
    /* insert new key */
    j = insert - split;
    right->key[j] = key;
    right->data[j] = data;
    /* left leaf number */
    leaf->entries = split;
}
static void leaf_simple_insert(bplus_leaf *leaf, key_t key, int data, int insert)
{
    int i;
    for (i = leaf->entries; i > insert; i--)
    {
        leaf->key[i] = leaf->key[i - 1];
        leaf->data[i] = leaf->data[i - 1];
    }
    leaf->key[i] = key;
    leaf->data[i] = data;
    leaf->entries++;
}

static int leaf_insert(bplustree *tree, bplus_leaf *leaf, key_t key, int data) //叶插入
{
    //搜索插入位置
    int insert = key_binary_search(leaf->key, leaf->entries, key);
    if (insert >= 0) //已经存在此点，直接返回错误
    {
        return -1;
    }
    insert = -insert - 1;
    //节点没满
    if (leaf->entries < tree->entries)
    {
        leaf_simple_insert(leaf, key, data, insert);
    }
    else //节点已满
    {

        int split = (tree->entries + 1) / 2; //向上取整
        //分割的兄弟节点
        bplus_leaf *slibing = leaf_new();
        // 根据定位信息，将原叶节点的数据复制到 兄弟节点中
        if (insert < split) //左分割
        {
            //将新数据加入左端兄弟
            leaf_split_left(leaf, slibing, key, data, insert);
        }
        else
        {
            /* code */
            leaf_split_right(leaf, slibing, key, data, insert);
        }
        //then 建立新的父节点
        if (insert < split)
        {

        }
        else
        {
            /* code */
        }
    }
}

int bplustree::parent_node_build(bplus_node *left, bplus_node *right, key_t key, int level)
{
    if (left->parent == NULL && right->parent == NULL)
    {
        //new parent
        bplus_non_leaf *parent = non_leaf_new();
        parent->key[0] = key;
        parent->sub_ptr[0] = left;
        left->parent = parent;
        left->parent_key_idx = -1;

        parent->sub_ptr[1] = right;
        right->parent = parent;
        right->parent_key_idx = 0;
        parent->children = 2;
        //updata root
        this->root = (bplus_node *)parent;
        this->level++;
        __list_add(&parent->link, &this->list[this->level], &this->list[this->level]);
        return 0;
    }
    else if(right->parent==NULL) //如果左节点有父节点，右节点为新节点将右节点的父节点设置为 左的父亲，非叶节点插入法
    {
        right->parent=left->parent;
        return non_leaf_insert(left->parent,left,right,key,level+1);
        
    }
    else
    {
        left->parent=right->parent;
        return non_leaf_insert(right->parent,left,right,key,level+1);
    }
    

}
int bplustree::non_leaf_insert(bplus_non_leaf *node,bplus_node *l_ch,bplus_node *r_ch,key_t key,int level)
{
    //搜索插入点
    
    
    //如果父节点没有满


    //父节点满了 ，进行节点分割
        //左分割
        n
        //右分割


        //建立新的父节点
}
static int bplus_tree_insert(bplustree *tree, key_t key, int data)
{
    bplus_node *node = tree->root;
    while (node != NULL)
    {
        if (!is_leaf(node)) //非叶子节点 按层次依次查找，直到叶节点
        {
            bplus_non_leaf *nln = (bplus_non_leaf *)node;
            int i = key_binary_search(nln->key, nln->children - 1, key);
            if (i >= 0)
            {
                node = nln->sub_ptr[i + 1];
            }
            else
            {
                i = -i - 1;
                node = nln->sub_ptr[i]; //要处理结果不同，通过将返回值设为正负不同的值，简化了
            }
        }
        else //叶子
        {
            /* code */
            bplus_leaf *ln = (bplus_leaf *)node;
            leaf_insert(tree, ln, key, data); //叶插入
        }
    }
    //如果root为null
    bplus_leaf *root = leaf_new();
    root->key[0] = key;
    root->data[0] = data;
    root->entries = 1;
    tree->root = (bplus_node *)root;
    __list_add(&tree->list[tree->level], &root->link, &tree->list[tree->level]);
    return 0;
}

int bplustree::bplus_tree_put(key_t key, int data)
{
    if (data)
    {
    }
}
