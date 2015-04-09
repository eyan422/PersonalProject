package accessarrayList;

import java.util.ArrayList;

public class AccessArrayList {
	public static void main(String args[]) {
		ArrayList<String> myArrList = new ArrayList<>();
		myArrList.add("One");
		myArrList.add("Two");
		myArrList.add("Four");
		myArrList.add(2, "Three");
		
		for (String element : myArrList) {
			System.out.println(element);
		}
	}
}