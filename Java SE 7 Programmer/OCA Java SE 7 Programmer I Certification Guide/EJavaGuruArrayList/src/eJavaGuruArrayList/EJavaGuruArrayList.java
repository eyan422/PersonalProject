/**
 * 
 */
package eJavaGuruArrayList;

import java.util.*; 
class EJavaGuruArrayList { 
	public static void main(String args[]) { 
		ArrayList<String> ejg = new ArrayList<>();
		
		ejg.add("One");
		ejg.add("Two");
		System.out.println(ejg.contains(new String("One")));
		System.out.println(ejg.indexOf("Two")); //1
		ejg.clear();
		System.out.println(ejg);
		//System.out.println(ejg.get(1));
	}
}
