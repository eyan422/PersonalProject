package arraylistsze;

import java.util.ArrayList;

public class ArrayListSize {
	public static void main(String args[]){
		ArrayList<String> myArrList = new ArrayList<String>();
		myArrList.add("One");
		myArrList.add("Two");
		
		String valFromList = myArrList.get(1);
		System.out.println(valFromList);
		System.out.println("Size:"+myArrList.size());
	}
}
