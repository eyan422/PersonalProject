package modifyarraylistwithstringbuilder;

import java.util.ArrayList;
public class ModifyArrayListWithStringBuilder {
	public static void main(String args[]) {
		ArrayList<StringBuilder> myArrList =
		new ArrayList<StringBuilder>();
		myArrList.add(new StringBuilder("One"));
		myArrList.add(new StringBuilder("Two"));
		myArrList.add(new StringBuilder("Three"));
		
		for (StringBuilder element : myArrList)
			element.append(element.length());
		
		for (StringBuilder element : myArrList)
			System.out.println(element);
	}
}