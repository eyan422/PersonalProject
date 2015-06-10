package rethrowexception2;

import java.io.*;
public class ReThrowException2 {
	public void myMethod() throws IOException {
		FileInputStream soccer = new FileInputStream("soccer.txt");
		soccer.close();
	}
}