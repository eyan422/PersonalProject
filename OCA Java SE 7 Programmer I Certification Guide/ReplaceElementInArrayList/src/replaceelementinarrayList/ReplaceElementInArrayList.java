package replaceelementinarrayList;

import java.util.ArrayList;
public class ReplaceElementInArrayList {
	public static void main(String args[]) {
		ArrayList<String> myArrList = new ArrayList<String>();
		myArrList.add("One");
		myArrList.add("Two");
		myArrList.add("Three");
		myArrList.set(1, "One and Half");
		
		for (String element:myArrList)
			System.out.println(element);
	}
}