package rethrowexception;

import java.io.*;

public class ReThrowException {
	FileInputStream soccer;
	
	public void myMethod() throws FileNotFoundException  {
		try {
			soccer = new FileInputStream("soccer.txt");
		}
		catch (FileNotFoundException fnfe) {
			throw fnfe;
		}
	}
}
