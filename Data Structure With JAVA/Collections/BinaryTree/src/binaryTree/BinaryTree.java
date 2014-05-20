package binaryTree;

import java.util.*;

public class BinaryTree<E> extends AbstractCollection<E> {
	protected E root;
	protected BinaryTree<E> left, right, parent;
	protected int size;
	
	public BinaryTree() {
	}
	
	public BinaryTree(E root) {
		this.root = root;
		size = 1;
	}
	
	public BinaryTree(E root, BinaryTree<E> left, BinaryTree<E> right) {
		this(root);
		
		if (left != null) {
			this.left = left;
			left.parent = this;
			size += left.size();
		}
		
		if (right != null) {
			this.right = right;
			right.parent = this;
			size += right.size();
		}
	}
	
	@SuppressWarnings({ "unchecked" })
	public boolean equals(Object object) {
		if (object == this) {
			return true;
		} else if (!(object instanceof BinaryTree)) {
			return false;
		}
		
		BinaryTree<E> that = (BinaryTree<E>) object;
		
		return that.root.equals(this.root)
		&& that.left.equals(this.left)
		&& that.right.equals(this.right)
		&& that.parent.equals(this.parent)
		&& that.size == this.size;
	}
	
	public int hashCode() {
		return root.hashCode() + left.hashCode() + right.hashCode() + size;
	}
	
	public int size() {
		return size;
	}
	
	public Iterator<E> iterator() {
		
		return new java.util.Iterator<E>() { // anonymous inner class
			private boolean rootDone;
			
			private Iterator<E> lIt, rIt; // child iterators
			
			public boolean hasNext() {
				return !rootDone || lIt != null && lIt.hasNext() || rIt != null && rIt.hasNext();
			}
			
			public E next() {
				if (rootDone) {
					if (lIt != null && lIt.hasNext()) {
						return lIt.next();
					}
					
					if (rIt != null && rIt.hasNext()) {
						return rIt.next();
					}
				
					return null;
				}
				
				if (left != null) {
					lIt = left.iterator();
				}
				
				if (right != null) {
					rIt = right.iterator();
				}
				
				rootDone = true;
				return root;	
			}
			
			public void remove() {
				throw new UnsupportedOperationException(); 
			}
		};
	}
}
