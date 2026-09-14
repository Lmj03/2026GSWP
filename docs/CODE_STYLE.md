# 코드 작성 규칙

직접 작성하는 C++ 소스·헤더와 GLSL 셰이더에 적용한다. 루트의 `.clang-format`을 기준으로 하며, clang-format 19.1.5로 정리했다. `.editorconfig`에도 들여쓰기와 파일 저장 형식을 지정했다.

- 중괄호는 여는 괄호와 닫는 괄호 모두 별도 줄에 놓는다(Allman 형식).
- 짧은 함수, 람다, 조건문, 반복문도 여러 줄로 작성한다. 조건문과 반복문은 본문이 한 문장이더라도 중괄호를 쓴다.
- 한 줄에 하나의 실행 문장을 작성한다.
- 들여쓰기는 탭 대신 공백 4칸을 사용한다.
- 함수 정의와 타입 정의 사이에는 빈 줄을 둔다. 함수 안에서도 처리 단계가 달라지면 빈 줄로 구분한다.
- 긴 호출과 선언은 약 100열을 기준으로 줄바꿈하고, 줄바꿈하는 인수와 매개변수는 각각 별도 줄에 놓는다. 문자열과 주석은 내용 보존을 위해 자동으로 쪼개지 않는다.
- 생성자 초기화 목록은 멤버별로 줄을 나눈다.
- 포인터와 참조 표기는 `Renderer*`, `const Settings&`처럼 타입에 붙인다.
- UTF-8과 CRLF 줄바꿈을 사용한다. 기존 include 순서는 유지한다. 특히 미리 컴파일된 헤더인 `stdafx.h`의 위치를 바꾸지 않는다.

```cpp
void UpdatePlayer(float deltaTime)
{
    if (deltaTime <= 0.f)
    {
        return;
    }

    MovePlayer(deltaTime);
    UpdateCamera(deltaTime);
}
```

저장소 루트에서 `clang-format -i --style=file SimpleGame/CityScene.cpp`처럼 대상 파일을 지정해 적용한다. 실행 파일이 PATH에 없다면 Visual Studio의 `VC/Tools/Llvm/bin/clang-format.exe`를 사용할 수 있다. 포맷 작업은 빌드나 게임 실행을 수행하지 않는다.

`SimpleGame/Dependencies`의 외부 라이브러리, 빌드 산출물, 게임 데이터와 문서는 C++ 포맷 대상에서 제외한다.
