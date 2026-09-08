/*
Copyright 2022 Lee Taek Hee (Tech University of Korea)
This program is free software: you can redistribute it and/or modify
it under the terms of the What The Hell License. Do it plz.
This program is distributed without any warranty.
*/

#include "stdafx.h"
#include "Renderer.h"
#include "CityScene.h"
#include "Dependencies/freeglut.h"
#include <algorithm>
#include <chrono>
#include <cctype>
#include <iostream>
#include <memory>
#include <Windows.h>

namespace
{
    std::unique_ptr<Renderer> renderer;
    CityScene city;
    bool keys[256] = {};
    bool running = true;
    HWND gameWindow = nullptr;
    std::chrono::steady_clock::time_point previous;

    void Display()
    {
        if (!running || !renderer) return;
        city.Draw(*renderer);
        glutSwapBuffers();
    }

    void Resize(int width, int height)
    {
        if (renderer) renderer->Resize(width,height);
    }

    void Close()
    {
        running=false;
        // FreeGLUT invokes this while the window's OpenGL context still exists.
        renderer.reset();
    }

    void Timer(int)
    {
        if (!running) return;
        const auto now=std::chrono::steady_clock::now();
        const float dt=std::min(.05f,std::chrono::duration<float>(now-previous).count());
        previous=now;
        // Do not retain movement keys when focus moves to another application.
        if (GetForegroundWindow()!=gameWindow)
            std::fill(keys,keys+256,false);
        else
        {
            keys[16]=(GetAsyncKeyState(VK_SHIFT)&0x8000)!=0;
            const unsigned char movement[]={'w','a','s','d'};
            for (unsigned char key : movement)
                keys[key]=(GetAsyncKeyState(static_cast<int>(std::toupper(key)))&0x8000)!=0;
        }
        city.Update(dt,keys);
        glutPostRedisplay();
        glutTimerFunc(16,Timer,0);
    }

    void KeyDown(unsigned char key,int,int)
    {
        key=static_cast<unsigned char>(std::tolower(key));
        if (keys[key]) return;
        keys[key]=true;
        switch (key)
        {
        case 27: Close(); glutLeaveMainLoop(); break;
        case 'e': city.Interact(); break;
        case 'r': city.ToggleRain(); break;
        case 'm': city.ToggleMap(); break;
        case 9: city.ToggleInterface(); break;
        case '+': case '=': city.Zoom(1); break;
        case '-': city.Zoom(-1); break;
        }
    }

    void KeyUp(unsigned char key,int,int)
    {
        keys[static_cast<unsigned char>(std::tolower(key))]=false;
    }

    void Special(int key,int,int)
    {
        if (key==GLUT_KEY_F2) city.Reset();
    }

    void Wheel(int,int direction,int,int) { city.Zoom(direction); }
}

int main(int argc,char** argv)
{
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);
    glutInit(&argc,argv);
    glutInitContextVersion(3,3);
    glutInitContextProfile(GLUT_CORE_PROFILE);
    glutInitDisplayMode(GLUT_DOUBLE|GLUT_RGBA);
    glutInitWindowSize(1280,800);
    glutInitWindowPosition(80,60);
    const int window=glutCreateWindow("NEON VEIL | Afterglow - OpenGL Prototype");
    if (window<=0) return 1;
    gameWindow=GetActiveWindow();
    if (gameWindow) SetWindowTextW(gameWindow,L"네온 베일 | 잔광 지구 - OpenGL 프로토타입");
    glutSetOption(GLUT_ACTION_ON_WINDOW_CLOSE,GLUT_ACTION_GLUTMAINLOOP_RETURNS);
    glewExperimental=GL_TRUE;
    const GLenum error=glewInit();
    if (error!=GLEW_OK || !GLEW_VERSION_3_3)
    {
        std::cerr << "OpenGL 3.3이 필요합니다. GLEW: " << glewGetErrorString(error) << '\n';
        glutDestroyWindow(window);
        return 1;
    }
    // Some GLEW versions leave GL_INVALID_ENUM while discovering a core context.
    while (glGetError()!=GL_NO_ERROR) {}
    renderer.reset(new Renderer(1280,800));
    if (!renderer->IsInitialized() || !city.Load())
    {
        std::cerr << "초기화에 실패했습니다. 위의 자원 또는 셰이더 오류를 확인하세요.\n";
        renderer.reset();
        glutDestroyWindow(window);
        return 1;
    }
    std::cout << "네온 베일 / OpenGL 3.3 / 계속 확장되는 도시\n"
        << "WASD 이동 | Shift 달리기 | E 도움/대화 | 마우스 휠 확대/축소\n"
        << "M 지도 | R 비 | Tab 안내 숨김 | F2 처음부터 | Esc 종료\n";
    glutDisplayFunc(Display);
    glutReshapeFunc(Resize);
    glutKeyboardFunc(KeyDown);
    glutKeyboardUpFunc(KeyUp);
    glutSpecialFunc(Special);
    glutMouseWheelFunc(Wheel);
    glutCloseFunc(Close);
    glutIgnoreKeyRepeat(1);
    previous=std::chrono::steady_clock::now();
    glutTimerFunc(16,Timer,0);
    glutMainLoop();
    // Close() released GPU resources before context destruction.
    return 0;
}
