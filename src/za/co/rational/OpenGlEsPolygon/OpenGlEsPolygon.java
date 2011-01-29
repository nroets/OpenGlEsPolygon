/* Copyright (C) 2011 Nic Roets. All rights reserved.

Redistribution and use in source and binary forms, with or without modification, are
permitted provided that the following conditions are met:

   1. Redistributions of source code must retain the above copyright notice, this list of
      conditions and the following disclaimer.

   2. Redistributions in binary form must reproduce the above copyright notice, this list
      of conditions and the following disclaimer in the documentation and/or other materials
      provided with the distribution.

THIS SOFTWARE IS PROVIDED BY Nic Roets ``AS IS'' AND ANY EXPRESS OR IMPLIED
WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND
FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL <COPYRIGHT HOLDER> OR
CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

The views and conclusions contained in the software and documentation are those of the
authors and should not be interpreted as representing official policies, either expressed
or implied, of Nic Roets.
*/
package za.co.rational.OpenGlEsPolygon;

import javax.microedition.khronos.egl.EGLConfig;
import javax.microedition.khronos.opengles.GL10;
import android.app.Activity;
import android.content.Context;
import android.opengl.GLSurfaceView;
import android.os.Bundle;

public class OpenGlEsPolygon extends Activity {
    @Override
    public void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        GLSurfaceView mGLView = new OpenGlEsPolygonView(this);
        setContentView(mGLView);
    } 

    static  {
        System.loadLibrary("openglespolygon");
    }
}

class OpenGlEsPolygonView extends GLSurfaceView {
    public OpenGlEsPolygonView(Context context) {
        super(context);
        OpenGlEsPolygonRenderer mRenderer = new OpenGlEsPolygonRenderer();//context);
        setRenderer(mRenderer);
    }
}

class OpenGlEsPolygonRenderer implements GLSurfaceView.Renderer {
    private int width, height;
    public void onSurfaceCreated(GL10 gl, EGLConfig config) {
        gl.glClearColor(.5f, .2f, .5f, 1);
     }
    public void onSurfaceChanged(GL10 gl, int w, int h) {
    	width = w;
    	height = h;
        gl.glViewport(0, 0, w, h);
    }
 
    public void onDrawFrame(GL10 gl) {
      	nativeRender(width, height);
    }
    private static native void nativeRender(int w, int h);
}

