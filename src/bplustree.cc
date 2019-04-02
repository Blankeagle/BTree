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
    else if (right->parent == NULL) //如果左节点有父节点，右节点为新节点将右节点的父节点设置为 左的父亲，非叶节点插入法
    {
        right->parent = left->parent;
        return non_leaf_insert(left->parent, left, right, key, level + 1);
    }
    else
    {
        left->parent = right->parent;
        return non_leaf_insert(right->parent, left, right, key, level + 1);
    }
}
int bplustree::non_leaf_insert(bplus_non_leaf *node, bplus_node *l_ch, bplus_node *r_ch, key_t key, int level)
{
    //搜索插入点
    int insert = key_binary_search(node->key, node->children - 1, key);
    assert(insert < 0);
    insert = -insert - 1; //不i能插入相同节点

    //如果父节点没有满
    if (node->children < this->order)
    {
        //简单插入
        non_leaf_simple_insert(node, l_ch, r_ch, key, insert);
    }
    else //节点分割
    {
        key_t split_key;
        int split = (node->children + 1) / 2; //分割点
        bplus_non_leaf *sibling = non_leaf_new();

        if (insert < split) //左分割
        {
            split_key = non_left_split_left(node, sibling, l_ch, r_ch, key, insert);
        }
        else if (insert == split)
        {
            split_key = non_leaf_split_right1(node, sibling, l_ch, r_ch, key, insert);
            /* code */
        }
        else
        {
            split_key = non_leaf_split_right2(node, sibling, l_ch, r_ch, key, insert);
        }
        //建立父节点
        if (insert < split)
        {
            return this->parent_node_build((bplus_node *)sibling, (bplus_node *)node, split_key, level);
        }
        else
        {
            return this->parent_node_build((bplus_node *)node, (bplus_node *)sibling, split_key, level);
        }
    }
    return 0;
}

static void non_leaf_simple_insert(bplus_non_leaf *node, bplus_node *l_ch, bplus_node *r_ch, key_t key, int insert)
{
    int i;
    for (i = node->children - 1; i > insert; i--) //右移动
    {
        node->key[i] = node->key[i - 1];
        node->sub_ptr[i + 1] = node->sub_ptr[i];
        node->sub_ptr[i + 1]->parent_key_idx = i;
    }
    node->key[i] = key;
    node->sub_ptr[i] = l_ch;
    l_ch->parent_key_idx = i - 1;

    node->sub_ptr[i + 1] = r_ch;
    r_ch->parent_key_idx = i;
    node->children++;
}

//目的是3分裂node节点，将left变为左节点，node为右节点，lch和rch是造成node分裂的直接元凶，此函数返回node的分裂key
//insert表示的是在node中的插入位置，此时已确定insert为node左半部分
static int non_left_split_left(bplus_non_leaf *node, bplus_non_leaf *left,
                               bplus_node *l_ch, bplus_node *r_ch, key_t key, int insert)
{
    int i, j, order = node->children; //最大ch数是度数，并不是key的数目
    key_t split_key;
    int split = (order + 1) / 2;
    __list_add(&left->link, node->link.prev, &node->link);

    //复制子节点指针到left
    for (i = 0, j = 0; i < split; i++, j++)
    {
        if (j == insert) //修改一条并新增一条记录
        {
            left->sub_ptr[j] = l_ch;
            l_ch->parent = left;
            l_ch->parent_key_idx = j - 1;
            left->sub_ptr[j + 1] = r_ch;
            r_ch->parent = left;
            r_ch->parent_key_idx = j;
            j++;
        }
        else
        {
            left->sub_ptr[j] = node->sub_ptr[i];
            left->sub_ptr[j]->parent = left;
            left->sub_ptr[j]->parent_key_idx = j - 1;
        }
        left->children = split;
    }
    //复制key
    for (i = 0, j = 0; i < split - 1; j++)
    {
        if (j = split)
        {
            left->key[j] = key;
        }
        else
        {
            left->key[j] = node->key[i];
            i++;
        }
    }
    if (insert == split - 1)
    {
        left->key[insert] = key;
        left->sub_ptr[insert] = l_ch;
        l_ch->parent = left;
        l_ch->parent_key_idx = j - 1;

        node->sub_ptr[0] = r_ch;
        split_key = key;
    }
    else
    {
        node->sub_ptr[0] = node->sub_ptr[split - 1];
        split_key = node->key[split - 2]; //
    }
    node->sub_ptr[0]->parent = node;
    node->sub_ptr[0]->parent_key_idx = -1;
    //将node中数据左移从split-1到children-1
    for (i = split - 1, j = 0; i < order - 1; i++, j++)
    {
        node->key[j] = node->key[i];
        node->sub_ptr[j + 1] = node->sub_ptr[i + 1];
        node->sub_ptr[j + 1]->parent = node;
        node->sub_ptr[j + 1]->parent_key_idx = j;
    }

    node->sub_ptr[j] = node->sub_ptr[i]; //没什么用啊能删吗？
    node->children = j + 1;
    return split_key;
}

static int non_leaf_split_right1(bplus_non_leaf *node, bplus_non_leaf *right,
                                 bplus_node *l_ch, bplus_node *r_ch, key_t key, int insert)
{
    int i, j, order = node->children;
    key_t split_key;
    int split = (order + 1) / 2;
    __list_add(&right->link, node->link.prev, node->link.next);
    split_key = node->key[split - 1];
    node->children = split;

    right->key[0] = key;
    right->sub_ptr[0] = l_ch;
    right->sub_ptr[0]->parent = right;
    l_ch->parent_key_idx = -1;
    right->sub_ptr[1] = r_ch;
    r_ch->parent = right;
    r_ch->parent_key_idx = 0;

    //复制指针
    for (i = split, j = 1; i < order - 1; i++, j++)
    {
        right->key[j] = node->key[i];
        right->sub_ptr[j + 1] = node->sub_ptr[i + 1];
        right->sub_ptr[j + 1]->parent = right;
        right->sub_ptr[j + 1]->parent_key_idx = j;
    }
    right->children = j + 1;
    return split_key;
}

static int non_leaf_split_right2(bplus_non_leaf *node, bplus_non_leaf *right,
                                 bplus_node *l_ch, bplus_node *r_ch, key_t key, int insert)
{
    int i, j, order = node->children;
    key_t split_key;
    /* split = [m/2] */
    int split = (order + 1) / 2;
    /* left node's children always be [split + 1] */
    node->children = split + 1;
    __list_add(&right->link, &node->link, node->link.next);
    split_key = node->key[split];
    right->sub_ptr[0] = node->sub_ptr[split + 1];
    right->sub_ptr[0]->parent = right;
    right->sub_ptr[0]->parent_key_idx = -1;
    //复制key值从split+1到order-1；
    for (i = split + 1, j = 0; i < order - 1; j++)
    {
        if (j != insert - split - 1)
        {
            right->key[j] = node->key[i];
            right->sub_ptr[j + 1] = node->sub_ptr[i + 1];
            right->sub_ptr[j + 1]->parent = right;
            right->sub_ptr[j + 1]->parent_key_idx = j;
            i++;
        }
    }

    //预留一个插入位
    if (j > insert - split - 1)
    {
        right->children = j + 1;
    }
    else
    {
        assert(j == insert - split - 1);
        right->children = j + 2;
    }
    //插入新的key和value
    j = insert - split - 1;
    right->key[j] = key;
    right->sub_ptr[j] = l_ch;
    l_ch->parent = right;
    l_ch->parent_key_idx = j - 1;
    right->sub_ptr[j + 1] = r_ch;
    r_ch->parent = right;
    r_ch->parent_key_idx = j;
    return split_key;
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

//****************************************************
//删除
int bplustree::bplus_tree_delete(key_t key)
{
    bplus_node *node = root;
    while (node != NULL)
    { //找到叶子节点并进行删除
        /* code */
        if (is_leaf(node))
        {
            bplus_leaf *ln = (bplus_leaf *)node;
            return leaf_remove(ln, key); //执行删除
        }
        else
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
                node = nln->sub_ptr[i];
            }
        }
    }
    return -1;
}

int bplustree::leaf_remove(bplus_leaf *leaf, key_t key)
{
    int remove = key_binary_search(leaf->key, leaf->entries, key);

    if (remove < 0)
    {
        return -1; //并不存在
    }
    if (leaf->entries > (entries + 1) / 2)
    {
        //简单删除
        leaf_simple_remove(leaf, remove);
    }
    else //节点调整
    {

        bplus_non_leaf *parent = leaf->parent;
        bplus_leaf *l_sib = list_prev_entry(leaf, link);
        bplus_leaf *r_sib = list_next_entry(leaf, link);
        if (parent != NULL)
        {
            //决定向哪个兄弟借
            int i = leaf->parent_key_idx; //
            // 选择函数
            if (leaf_sibling_select(l_sib, r_sib, parent, i) == LEFT_SIBLING) //和左兄弟。。
            {
                if (l_sib->entries > (this->entries + 1) / 2)
                { //向左节点借一个
                    leaf_shift_from_left(leaf, l_sib, i, remove);
                }
                else
                {
                    //合并两个节点
                    leaf_merge_into_left(leaf, l_sib, remove);
                    //删除父节点的值
                    non_leaf_remove(parent, i);
                }
            }
            else
            {
                //和同级合并时发生溢出，先删除

                /* code */
                leaf_simple_remove(leaf, remove);
                if (r_sib->entries > (this->entries + 1) / 2)
                {
                    leaf_shift_from_right(leaf, r_sib, i + 1);
                }
                else
                {
                    /* code */
                    leaf_merge_from_right(leaf, r_sib);
                    non_leaf_remove(parent, i + 1);
                }
            }
        }
    }
    return 0;
}

static void leaf_merge_from_right(bplus_leaf *leaf, bplus_leaf *right)
{
    int i, j;
    /* merge from right sibling */
    for (i = leaf->entries, j = 0; j < right->entries; i++, j++)
    {
        leaf->key[i] = right->key[j];
        leaf->data[i] = right->data[j];
    }
    leaf->entries = i;
    /* delete right sibling */
    leaf_del(right);
}

static void leaf_shift_from_right(bplus_leaf *leaf, bplus_leaf *right, int parent_key_index)
{
    int i;
    //将右兄弟的第一个节点借给它
    leaf->key[leaf->entries] = right->key[0];
    leaf->data[leaf->entries] = right->data[0];
    leaf->entries++;

    //右兄弟节点左移数据

    for (i = 0; i < right->entries - 1; i++)
    {
        right->key[i] = right->key[i + 1];
        right->data[i] = right->data[i + 1];
    }
    right->entries--;
    //更新父节点信息
    leaf->parent->key[parent_key_index] = right->key[0];
}

//*********************非叶节点删除操作
//删除非叶节点中的值
void bplustree::non_leaf_remove(bplus_non_leaf *node, int remove) // 删除node节点的，remove下标数据
{
    if (node->children > (this->order + 1) / 2)
    {
        //简单删除
        non_leaf_remove(node, remove);
    }
    else //合并或 数据借
    {
        /* code */
        bplus_non_leaf *l_sib = list_prev_entry(node, link);
        bplus_non_leaf *r_sib = list_next_entry(node, link);
        bplus_non_leaf *parent = node->parent;
        if (parent != NULL)
        {
            int i = node->parent_key_idx;
            if (LEFT_SIBLING == non_leaf_sibling_select(l_sib, r_sib, parent, i))
            {
                if (l_sib->children > (this->order + 1) / 2)
                {
                    non_leaf_shift_from_left(node, l_sib, i, remove);
                }
                else
                {
                    /* code */
                    non_leaf_merge_into_left(node, l_sib, i, remove);
                    non_leaf_remove(parent, i);
                }
            }
            else
            {
                /* code */
                non_leaf_simple_remove(node, remove);
                if (r_sib->children > (this->order + 1) / 2)
                {
                    non_leaf_shift_from_right(node, r_sib, i + 1); //right节点的父节点索引是i+1
                }
                else
                {

                    /* code */
                    non_leaf_merge_from_right(node, r_sib, i + 1);
                    non_leaf_remove(parent, i + 1);
                }
            }
        }
    }
}

static void non_leaf_merge_from_right(struct bplus_non_leaf *node, struct bplus_non_leaf *right,
                                      int parent_key_index)
{
    int i, j;
    node->key[node->children - 1] = node->parent->key[parent_key_index];
    node->children++;
    //merge from right
    for (i = node->children - 1, j = 0; j < right->children - 1; i++, j++)
    {
        node->key[i] = right->key[j];
    }
    for (i = node->children - 1, j = 0; j < right->children; i++, j++) //注意subptr比key多一个
    {
        node->sub_ptr[i] = right->sub_ptr[j];
        node->sub_ptr[i]->parent = node;
        node->sub_ptr[i]->parent_key_idx = i - 1;
    }
    node->children = i;

    non_leaf_del(right);
}
static void non_leaf_shift_from_right(struct bplus_non_leaf *node, struct bplus_non_leaf *right,
                                      int parent_key_index)
{
    int i;
    node->key[node->children - 1] = node->parent->key[parent_key_index];
    node->parent->key[parent_key_index] = right->key[0];

    node->sub_ptr[node->children] = right->sub_ptr[0];
    node->sub_ptr[node->children]->parent = node;
    node->sub_ptr[node->children]->parent_key_idx = node->children - 1;
    node->children++;

    //将right中数据左移
    for (i = 0; i < right->children - 2; i++)
    {
        right->key[i] = right->key[i + 2];
    }
    for (i = 0; i < right->children - 2; i++)
    {
        right->sub_ptr[i] = right->sub_ptr[i + 1];
        right->sub_ptr[i]->parent_key_idx = i - 1;
    }
    right->children--;
}

static void non_leaf_merge_into_left(struct bplus_non_leaf *node, struct bplus_non_leaf *left,
                                     int parent_key_index, int remove)
{
    int i, j;
    left->key[left->children - 1] = node->parent->key[parent_key_index];
    //copy key
    for (i = left->children, j = 0; j < node->children - 1; j++)
    {
        if (j != remove)
        {
            left->key[i] = node->key[j];
            i++;
        }
    }

    //copy pointer
    for (i = left->children, j = 0; j < node->children; j++)
    {
        if (j != remove + 1)
        {
            left->sub_ptr[i] = node->sub_ptr[j];
            left->sub_ptr[i]->parent = left;
            left->sub_ptr[i]->parent_key_idx = i - 1;
            i++;
        }
    }
    left->children = i;
    //删除空节点
    non_leaf_del(node);
}
static void non_leaf_shift_from_left(bplus_non_leaf *node, bplus_non_leaf *left,
                                     int parent_key_index, int remove)
{
    int i;

    //向右移动数据
    for (i = remove; i > 0; i--)
    {
        node->key[i] = node->key[i + 1];
    }
    for (i = remove + 1; i > 0; i--)
    {
        node->sub_ptr[i] = node->sub_ptr[i - 1];
        node->sub_ptr[i]->parent_key_idx = i - 1;
    }
    //
    node->key[0] = node->parent->key[parent_key_index];
    node->parent->key[parent_key_index] = left->key[left->children - 2];

    node->sub_ptr[0] = left->sub_ptr[left->children - 1];
    node->sub_ptr[0]->parent = node;
    node->sub_ptr[0]->parent_key_idx = -1;
    left->children--;
}
static int non_leaf_sibling_select(bplus_non_leaf *l_sib, bplus_non_leaf *r_sib,
                                   bplus_non_leaf *parent, int i)
{
    if (i == -1)
    {
        return RIGHT_SIBING;
    }
    else if (parent->children - 2 == i)

    {

        return LEFT_SIBLING;
    }
    else
    {
        return l_sib->children >= r_sib->children ? LEFT_SIBLING : RIGHT_SIBING;
    }
}
static void non_leaf_simple_remove(bplus_non_leaf *node, int remove)
{
    assert(node->children >= 2);
    for (; remove < node->children - 2; remove++)
    {
        node->key[remove] = node->key[remove + 1];
        node->sub_ptr[remove + 1] = node->sub_ptr[remove + 2];
        node->sub_ptr[remove + 1]->parent_key_idx = remove;
    }
    node->children--;
}

//*********************非叶节点删除操作

//*********************y叶节点的删除操作

//和左边的节点合并
static void leaf_merge_into_left(bplus_leaf *leaf, bplus_leaf *left, int remove)
{
    int i, j;
    for (i = left->entries, j = 0; j < leaf->entries; j++)
    {
        if (j != remove)
        {
            left->key[i] = leaf->key[j];
            left->data[i] = leaf->data[j];
            i++;
        }
    }
    left->entries = i;
    leaf_del(leaf);
}
//向左兄弟借一个
static void leaf_shift_from_left(bplus_leaf *leaf, bplus_leaf *left, int parent_key_index, int remove)
{

    //右移节点数据
    for (; remove > 0; remove--)
    {
        leaf->key[remove] = leaf->key[remove - 1];
        leaf->data[remove] = leaf->data[remove - 1];
    }
    //从左兄弟节点中借最后一个数据放入此节点
    leaf->key[0] = left->key[left->entries - 1]; //？？？？  ==left->left->key[left->children - 2]
    leaf->data[0] = left->data[left->entries - 1];
    left->entries--;
    //更新父节点
    leaf->parent->key[parent_key_index] = leaf->key[0];
}

//删除数据时，数据数小于1/2选择左右兄弟的哪个和并
static int leaf_sibling_select(bplus_leaf *l_sib, bplus_leaf *r_sib, bplus_non_leaf *parent, int i)
{
    if (i == -1)
    {
        //第一个子节点，没有左兄弟，直接选择右节点
        return RIGHT_SIBING;
    }
    else if (parent->children - 2 == i)
    {
        /* code */
        return LEFT_SIBLING;
    }
    else
    {
        //选子节点更多的那个合并
        return l_sib->entries >= r_sib->entries ? LEFT_SIBLING : RIGHT_SIBING;
    }
}

void leaf_simple_remove(bplus_leaf *leaf, int remove)
{
    for (; remove < leaf->entries - 1; remove++) //数据左移
    {
        leaf->key[remove] = leaf->key[remove + 1];
        leaf->data[remove] = leaf->data[remove + 1];
    }
    leaf->entries--;
}

void leaf_del(bplus_leaf *node)
{
    list_del(&node->link);
    free(node);
}
void non_leaf_del(bplus_non_leaf *node)
{
    list_del(&node->link);
    free(node);
}
//*********************y叶节点的删除操作

//&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&
int bplustree::bplus_tree_put(key_t key, int data) // 插入数据和删除
{
    if (data)
    {
        return bplus_tree_insert(this, key, data);
    }
    else
    {
    }
}
