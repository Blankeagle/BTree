
// # @File:  bplustree.h
// # @Brief
// # BTreeplus的实现类
// # @Author Jicheng Zhu
// # @Email 2364464666@qq.com
// #

#ifndef _BPLUS_TREE_H
#define _BPLUS_TREE_H

#define PLUSE_MIN_ORDER 3      //非叶节点的最小阶数
#define PLUSE_MAX_ORDER 64     //最大
#define PLUSE_MAX_ENTERIRES 64 //叶子节点的最大条目数
#define PLUSE_MAX_LEVEL 10     //树的最大高度

typedef int key_t; // 键值类型
struct list_head
{
    /* data */
    struct list_head *prev, *next;
};
static inline void list_init(struct list_head *link) //也就是说建议编译器将指定的函数体插入并取代每
{                                                    //一处调用该函数的地方（上下文），从而节省了每次调用函数带来的额外时间开支。
    link->prev = link;
    link->next = link;
}

static inline void __list_add(struct list_head *link, struct list_head *prev, list_head *next)
{
    link->next = next;
    link->prev = prev;
    next->prev = link;
    prev->next = link;
}
static inline void __list_del(list_head *prev, list_head *next)
{
    prev->next = next;
    next->prev = prev;
}

struct bplus_node //通用型节点
{
    /* data */
    int type;
    int parent_key_idx;            //父节点中的父索引号
    struct bplus_non_leaf *parent; //父节点 ，
    struct list_head link;         //每一层的节点连接起来
};
struct bplus_non_leaf //非叶子节点
{
    /* data */
    int type;
    int parent_key_idx;
    struct bplus_non_leaf *parent;
    struct list_head link;
    int children;
    int key[PLUSE_MAX_ORDER - 1];
    struct bplus_node *sub_ptr[PLUSE_MAX_ORDER]; //子节点指针要比key多一个
};
struct bplus_leaf //叶子节点
{
    /* data */
    int type;
    int parent_key_idx;
    struct bplus_non_leaf *parent;
    struct list_head link;
    int entries;  //数据个税
    int key[PLUSE_MAX_ENTERIRES];
    int data[PLUSE_MAX_ENTERIRES]; //数据，一个key 对应一个数据
};

class bplustree
{

  public:
    int order;

    int entries;
    int level;
    struct bplus_node *root;
    struct list_head list[PLUSE_MAX_LEVEL];

  private:
    /* data */
  public:
    bplustree();
    ~bplustree();
    int bplus_tree_dump();                            //遍历所有元数据
    int bplus_tree_get(key_t key);                    //查找数据
    int bplus_tree_put(key_t key, int data);          //插入数据
    int bplus_tree_get_range(key_t key1, key_t key2); //
    int parent_node_build(bplus_node *left,bplus_node *right,key_t key,int level); //
    int non_leaf_insert(bplus_non_leaf *node,bplus_node *l_ch,bplus_node *r_ch,key_t key,int level);
    void init(int order, int entries);
};

void bplustree::init(int order, int enteies)
{
    this->order = order;
    this->entries = enteies;
}
bplustree::bplustree()
{
}

bplustree::~bplustree()
{

}

#endif //_BPlus_TREE_H