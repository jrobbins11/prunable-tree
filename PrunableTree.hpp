#ifndef __PRUNABLE_TREE_HPP___
#define __PRUNABLE_TREE_HPP___

#include <memory_resource>
#include <vector>
#include <new>
#include <iostream>
#include <utility>
#include <algorithm>
#include <sstream>

struct TreeNode
{
    int ind = -1; // index for fixed value
    bool value = false; // false -> low, true -> high

    TreeNode* firstchild = nullptr;
    TreeNode* nextsibling = nullptr;
    TreeNode* previous = nullptr;
};

class NodePool
{
    public:
        NodePool()
        {
            this->nodes_allocated = 0; // init
        }

        TreeNode* new_node(int ind, bool value) 
        {
            void* mem = pool.allocate(sizeof(TreeNode), alignof(TreeNode));
            TreeNode* node = new (mem) TreeNode;
            node->ind = ind;
            node->value = value;
            ++this->nodes_allocated;
            return node;
        }

        void delete_node(TreeNode* node) 
        {
            // do not need to call destructor for POD types
            if (!node) return;
            pool.deallocate(node, sizeof(TreeNode), alignof(TreeNode));
            --this->nodes_allocated;
        }

        void release() // Clear the pool, all allocated nodes are invalid 
        {
            pool.release();
        }

        size_t size() const
        {
            return this->nodes_allocated;
        }

    private:
        std::pmr::unsynchronized_pool_resource pool;
        size_t nodes_allocated; // number of nodes allocated
};

struct BranchInfo
{
    TreeNode* node; // node
    std::vector<std::pair<int, bool>> delta_bins; // leaf binaries
};

class Tree
{
    public:
        // constructors
        Tree()
        {
            this->root = node_pool.new_node(-1, false); // empty root
            this->n_bins = 0; // set number of bins
        }
        
        Tree(int ind, bool value, int n_bins)
        {
            this->root = node_pool.new_node(ind, value);
            this->n_bins = n_bins; 
            if (ind >= 0)
                this->leaves.push_back({this->root, {{ind, value}}}); // add to leaves
        }

        Tree(const std::vector<std::vector<std::pair<int, bool>>>& leaf_bins)
        {
            // input validity checking
            if (leaf_bins.empty())
            {    
                Tree();
                return;
            }
            
            int n_bins = leaf_bins[0].size(); // number of bins
            for (const auto& leaf : leaf_bins)
            {
                if (leaf.size() != n_bins)
                    throw std::invalid_argument("All leaves must have the same number of binaries");
            }
            this->n_bins = n_bins; // set number of bins

            // manually build tree
            this->root = node_pool.new_node(-1, false); // empty root
            build_from_leaves_helper(this->root, leaf_bins, 0);
        }

        Tree(const Tree& other) // copy 
        {
            this->n_bins = other.n_bins;   
            this->root = traverse_and_copy(other.root, nullptr, std::vector<std::pair<int, bool>>());
        }

        // copy assignment operator
        Tree& operator=(const Tree& other)
        {
            if (this != &other) // self-assignment check
            {
                this->node_pool.release(); // clear node pool
                this->leaves.clear(); // clear leaves
                this->n_bins = other.n_bins; // copy number of bins
                this->root = traverse_and_copy(other.root, nullptr, std::vector<std::pair<int, bool>>()); // copy tree
            }
            return *this;
        }

        // get leaf binaries
        std::vector<std::vector<std::pair<int, bool>>> get_leaf_bins() const
        {
            std::vector<std::vector<std::pair<int, bool>>> leaf_bins; // init
            for (const auto& leaf : this->leaves)
            {
                leaf_bins.push_back(leaf.second);
            }
            return leaf_bins;
        }

        // prune from node
        void prune(TreeNode* node)
        {
            prune_down(node->firstchild); // delete children
            prune_up(node); // delete node
        }

        // prune tree from given leaf indices
        void prune_leaves(const std::vector<int>& leaf_indices)
        {
            // get nodes for each leaf
            std::vector<TreeNode*> leaf_nodes;
            for (int ind : leaf_indices)
            {
                if (ind < 0 || ind >= this->leaves.size())
                    throw std::out_of_range("Leaf index out of range");
                leaf_nodes.push_back(this->leaves[ind].first);
            }

            // delete leaves and update tree
            auto it = this->leaves.begin();
            while (it != this->leaves.end())
            {
                if (std::find(leaf_nodes.begin(), leaf_nodes.end(), it->first) != leaf_nodes.end())
                {
                    // recursively delete nodes
                    prune(it->first);
                    
                    // remove leaf from leaves
                    it = this->leaves.erase(it); // remove from leaves
                }
                else
                {
                    ++it; // move to next leaf
                }
            }
        }

        // root node
        TreeNode* get_root() const
        {
            return this->root;
        }

        // get subtrees from provided subtree
        std::vector<BranchInfo> get_branch_info(const TreeNode* node) const
        {
            return get_branch_info_helper(node, std::vector<std::pair<int, bool>>());
        }

        // get methods
        size_t get_n_nodes() const
        {
            return this->node_pool.size(); // return number of nodes
        }

        size_t get_n_bins() const
        {
            return this->n_bins; // return number of bins
        }
        
        // friend function declarations
        friend Tree vcat(const Tree& tree1, const Tree& tree2);
        friend Tree hcat(const std::vector<Tree>& trees);
        friend std::ostream& operator<<(std::ostream& os, const Tree& tree);

        #ifdef PRUNABLE_TREE_DEBUG
        std::vector<std::vector<std::pair<int, bool>>> get_leaf_bins_propagate() const
        {
            std::vector<std::vector<std::pair<int, bool>>> leaf_bins; // init
            get_leaf_bins_propagate_helper(this->root, leaf_bins, std::vector<std::pair<int, bool>>());
            return leaf_bins;
        }

        std::string print_propagated_leaves() const
        {
            std::stringstream ss;
            std::vector<std::vector<std::pair<int, bool>>> leaf_bins = get_leaf_bins_propagate();
            for (auto it=leaf_bins.begin(); it!=leaf_bins.end(); ++it)
            {
                std::sort(it->begin(), it->end(), [](const std::pair<int, bool>& a, const std::pair<int, bool>& b)
                {
                    return a.first < b.first;
                });
            }

            for (const auto& leaf : leaf_bins)
            {
                ss << "Leaf: ";
                for (const auto& bin : leaf)
                {
                    ss << "(" << bin.first << ", " << bin.second << ") ";
                }
                ss << std::endl;
            }
            return ss.str();
        }
        #endif        

    private:
        NodePool node_pool;
        TreeNode* root = nullptr;
        std::vector<std::pair<TreeNode*, std::vector<std::pair<int, bool>>>> leaves;
        int n_bins = 0; // number of variables

        // shift index function
        int shift_index(int ind, int offset)
        {
            return (ind < 0 ? ind : ind + offset);
        }
        
        // traverse and copy
        TreeNode* traverse_and_copy(TreeNode* copy_node, TreeNode* prev_node, const std::vector<std::pair<int, bool>>& bins, int offset=0)
        {
            if (!copy_node)
            {
                this->leaves.push_back(std::make_pair(prev_node, bins)); // add to leaves
                return nullptr;
            }

            // check for empty node
            if (prev_node && copy_node->ind < 0)
                return traverse_and_copy(copy_node->firstchild, prev_node, bins, offset); // skip empty node

            // copy current node
            TreeNode* new_node = node_pool.new_node(shift_index(copy_node->ind, offset), copy_node->value);
            new_node->previous = prev_node; // set previous node

            // copy siblings if they exist
            if (copy_node->nextsibling)
                new_node->nextsibling = traverse_and_copy(copy_node->nextsibling, new_node, bins, offset);

            // add binaries
            std::vector<std::pair<int, bool>> bins_copy = bins; // copy
            if (copy_node->ind >= 0) // check if non-empty
                bins_copy.push_back(std::make_pair(new_node->ind, new_node->value)); // add current node

            // recursively copy children
            new_node->firstchild = traverse_and_copy(copy_node->firstchild, new_node, bins_copy, offset);

            return new_node;
        }

        // tree pruning helpers
        void prune_up(TreeNode* node)
        {
            // do not prune if node is null or has children
            if (!node || node->firstchild) return;

            // upstream node
            TreeNode* prev = node->previous;

            if (prev && prev->firstchild == node)
            {
                // update parent connectivity
                prev->firstchild = node->nextsibling;
                
                // recurse if able
                if (!prev->firstchild)
                    prune_up(prev);
            }
            else
            {
                // update sibling connectivity
                prev->nextsibling = node->nextsibling;
            }

            // delete node
            this->node_pool.delete_node(node);
        }

        void prune_down(TreeNode* node)
        {
            if (!node) return;

            TreeNode* child = node->firstchild;
            TreeNode* sibling = node->nextsibling;

            prune_down(child); // recurse down
            prune_down(sibling); // recurse across
        }

        // helper for branch info method
        std::vector<BranchInfo> get_branch_info_helper(const TreeNode* node, const std::vector<std::pair<int, bool>>& bins) const
        {
            // init output
            std::vector<BranchInfo> children_info; // init

            // loop through children
            if (node->firstchild)
            {
                TreeNode* child = node->firstchild;
                while (child)
                {
                    BranchInfo info;
                    info.node = child; // set node
                    info.delta_bins = bins;
                    if (child->ind >= 0) // check if non-empty
                        info.delta_bins.push_back(std::make_pair(child->ind, child->value)); // add current node
                    children_info.push_back(info); // add child
                    child = child->nextsibling; // move to next sibling
                }
            }

            // if only one child and not a leaf, recurse
            if (children_info.size() == 1 && children_info[0].node->firstchild)
            {   
                return get_branch_info_helper(children_info[0].node, children_info[0].delta_bins);
            }
            return children_info;
        }

        // build tree from leaves
        void build_from_leaves_helper(TreeNode* node, const std::vector<std::vector<std::pair<int, bool>>>& leaf_bins, int bin_ind)
        {
            if (!node) return; // invalid

            // leaf
            if (bin_ind >= this->n_bins)
            {
                this->leaves.push_back(std::make_pair(node, leaf_bins[0])); // add to leaves
                return;
            }

            // get leaves corresponding to high and low values at the given index
            std::vector<std::vector<std::pair<int, bool>>> low_leaves, high_leaves;
            for (auto it=leaf_bins.begin(); it!=leaf_bins.end(); ++it)
            {
                if (it->at(bin_ind).second)
                    high_leaves.push_back(*it);
                else
                    low_leaves.push_back(*it);
            }

            // create new nodes and recurse
            if (low_leaves.size() > 0 && high_leaves.size() > 0)
            {
                TreeNode* low_node = node_pool.new_node(bin_ind, false);
                TreeNode* high_node = node_pool.new_node(bin_ind, true);
                node->firstchild = low_node;
                low_node->nextsibling = high_node;
                low_node->previous = node;
                high_node->previous = low_node;

                build_from_leaves_helper(low_node, low_leaves, bin_ind+1);
                build_from_leaves_helper(high_node, high_leaves, bin_ind+1);
            }
            else if (low_leaves.size() > 0)
            {
                TreeNode* low_node = node_pool.new_node(bin_ind, false);
                node->firstchild = low_node;
                low_node->previous = node;
                build_from_leaves_helper(low_node, low_leaves, bin_ind+1);
            }
            else if (high_leaves.size() > 0)
            {
                TreeNode* high_node = node_pool.new_node(bin_ind, true);
                node->firstchild = high_node;
                high_node->previous = node;
                build_from_leaves_helper(high_node, high_leaves, bin_ind+1);
            }
            else
            {
                return;
            }
        }

        #ifdef PRUNABLE_TREE_DEBUG
        void get_leaf_bins_propagate_helper(const TreeNode* node, std::vector<std::vector<std::pair<int, bool>>>& leaves,
            std::vector<std::pair<int, bool>> bins) const
        {
            if (!node) return;

            if (node->nextsibling)
                get_leaf_bins_propagate_helper(node->nextsibling, leaves, bins);

            if (node->ind >= 0) // check if non-empty
                bins.push_back(std::make_pair(node->ind, node->value));
            
            if (node->firstchild)
                get_leaf_bins_propagate_helper(node->firstchild, leaves, bins);
            else
                leaves.push_back(bins);
        }
        #endif
};


// vertical concatenation
Tree vcat(const Tree& tree1, const Tree& tree2)
{
    // init new tree
    Tree new_tree = tree1; // copy constructor
    new_tree.n_bins += tree2.n_bins; // update number of bins

    // traverse and copy from leaves
    std::vector<std::pair<TreeNode*, std::vector<std::pair<int, bool>>>> old_leaves = new_tree.leaves; // copy
    new_tree.leaves.clear(); // clear leaves
    for (auto& leaf : old_leaves)
    {
        leaf.first->firstchild = new_tree.traverse_and_copy(tree2.root, leaf.first, leaf.second, tree1.n_bins);
    }
    
    return new_tree;
}

// horizontal concatenation
Tree hcat(const std::vector<Tree>& trees)
{
    // track new binary variables
    int n_bins = 0; // init
    std::vector<int> new_bins; // init
    for (auto& tree : trees)
    {
        new_bins.push_back(n_bins + tree.n_bins);
        n_bins += tree.n_bins+1; // update total number of binaries
    }

    // init new tree
    Tree new_tree(-1, false, n_bins); // empty root
    new_tree.leaves.clear();

    // manually add children
    for (size_t i=0; i<trees.size(); i++)
    {
        // first child
        if (i == 0)
        {
            TreeNode* node = new_tree.node_pool.new_node(new_bins[i], true);
            node->previous = new_tree.root;
            new_tree.root->firstchild = node;
            new_tree.leaves.push_back({node, {{node->ind, node->value}}});
        }
        else
        {
            TreeNode* node = new_tree.node_pool.new_node(new_bins[i], true);
            node->previous = new_tree.root;
            new_tree.leaves[i-1].first->nextsibling = node;
            new_tree.leaves.push_back({node, {{node->ind, node->value}}});
        }
    }

    // traverse and copy from leaves
    std::vector<std::pair<TreeNode*, std::vector<std::pair<int, bool>>>> old_leaves = new_tree.leaves; // copy
    new_tree.leaves.clear(); // clear leaves
    n_bins = 0; // re-init
    for (size_t i=0; i<old_leaves.size(); i++)
    {
        old_leaves[i].first->firstchild = new_tree.traverse_and_copy(trees[i].root, old_leaves[i].first, old_leaves[i].second, n_bins);
        n_bins += trees[i].n_bins + 1; // update total number of binaries
    }

    return new_tree;
}


// print tree
std::ostream& operator<<(std::ostream& os, const Tree& tree)
{
    // get bins and sort
    std::vector<std::vector<std::pair<int, bool>>> leaf_bins = tree.get_leaf_bins();

    for (auto it=leaf_bins.begin(); it!=leaf_bins.end(); ++it)
    {
        std::sort(it->begin(), it->end(), [](const std::pair<int, bool>& a, const std::pair<int, bool>& b)
        {
            return a.first < b.first;
        });
    }

    // print
    os << "Prunable Tree: "<< std::endl;
    os << "  n_bins = " << tree.n_bins << ", n_leaves = " << tree.leaves.size() << ", n_nodes = " << tree.node_pool.size() << std::endl;
    os << "  Leaf bins: " << std::endl;
    for (const auto& leaf : leaf_bins)
    {
        os << "    Leaf: ";
        for (const auto& bin : leaf)
        {
            os << "(" << bin.first << ", " << bin.second << ") ";
        }
        os << std::endl;
    }
    return os;
}


#endif