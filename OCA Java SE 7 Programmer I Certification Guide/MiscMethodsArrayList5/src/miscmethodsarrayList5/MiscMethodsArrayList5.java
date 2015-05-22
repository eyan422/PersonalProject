package miscmethodsarrayList5;

import java.util.ArrayList;

public class MiscMethodsArrayList5 {
	public static void main(String args[]) {
		ArrayList<StringBuilder> myArrList = new ArrayList<StringBuilder>();
		
		StringBuilder sb1 = new StringBuilder("Jan");
		StringBuilder sb2 = new StringBuilder("Feb");
		
		myArrList.add(sb1);
		myArrList.add(sb2);
		myArrList.add(sb2);
		
		ArrayList<StringBuilder> assignedArrList = myArrList;
		ArrayList<StringBuilder> clonedArrList = (ArrayList<StringBuilder>)myArrList.clone();
		
		System.out.println(myArrList == assignedArrList);
		System.out.println(myArrList == clonedArrList);
		
		StringBuilder myArrVal = myArrList.get(0);
		StringBuilder assignedArrVal = assignedArrList.get(0);
		StringBuilder clonedArrVal = clonedArrList.get(0);
		
		System.out.println(myArrVal == assignedArrVal);
		System.out.println(myArrVal == clonedArrVal);
		
		System.out.println(myArrList.toArray()[0]);
		System.out.println(myArrList.toArray()[1]);
		System.out.println(myArrList.toArray()[2]);
		//System.out.println(myArrList.toArray()[3]);
	}
}
