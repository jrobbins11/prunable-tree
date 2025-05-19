#include <iostream>

#define PRUNABLE_TREE_DEBUG
#include "PrunableTree.hpp"

int main()
{
    std::stringstream ss;

    Tree tree1(0, true, 1);
    Tree tree2(0, false, 1);

    Tree tree = hcat({tree1, tree2});
    tree = vcat(tree, tree);
    tree = hcat({tree, tree});

    std::cout << tree << std::endl;

    tree.prune_leaves({0, 1, 2, 3, 5});

    std::cout << tree << std::endl;
    std::cout << "from forward propagation: " << std::endl << tree.print_propagated_leaves() << std::endl;

    return 0;
}