package deleteelementsfromarrayList;

import java.util.ArrayList;
public class DeleteElementsFromArrayList {
	public static void main(String args[]) {
		ArrayList<StringBuilder> myArrList = new ArrayList<>();
		
		StringBuilder sb1 = new StringBuilder("One");
		StringBuilder sb2 = new StringBuilder("Two");
		StringBuilder sb3 = new StringBuilder("Three");
		StringBuilder sb4 = new StringBuilder("Four");
		
		myArrList.add(sb1);
		myArrList.add(sb2);
		myArrList.add(sb3);
		myArrList.add(sb4);
		myArrList.remove(1);
		
		for (StringBuilder element:myArrList)
			System.out.println(element);
		
		myArrList.remove(sb3);
		myArrList.remove(new StringBuilder("Four"));
		System.out.println();
		
		for (StringBuilder element : myArrList)
			System.out.println(element);
	}
}