package binaryTree;

public class TestBinaryTree {
	static public void main(String[] args) {
		
		BinaryTree<String> e = new BinaryTree<String>("E");
		BinaryTree<String> g = new BinaryTree<String>("G");
		BinaryTree<String> h = new BinaryTree<String>("H");
		BinaryTree<String> i = new BinaryTree<String>("I");
		BinaryTree<String> d = new BinaryTree<String>("D", null, g);
		BinaryTree<String> f = new BinaryTree<String>("F", h, i);
		BinaryTree<String> b = new BinaryTree<String>("B", d, e);
		BinaryTree<String> c = new BinaryTree<String>("C", f, null);
		BinaryTree<String> tree = new BinaryTree<String>("A", b, c);
		
		System.out.printf("tree: %s", tree);
	}
}
