package com.vperflab.tsserver;

import java.lang.annotation.*;

@Retention(RetentionPolicy.RUNTIME)
@Target({ElementType.METHOD})
public @interface TSServerMethod
{
    String name();
    String[] argNames() default {};
    boolean noReturn() default false; 
}
