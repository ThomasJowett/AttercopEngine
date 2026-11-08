#pragma once
#include "Asset/LuaScript.h"
#include "Scripting/Lua/LuaApiEntry.h"

#include "ImGui/ImGuiTextEditor.h"

#include "Interfaces/ICopyable.h"
#include "Interfaces/IUndoable.h"
#include "Interfaces/ISaveable.h"
#include "ViewerManager.h"

class ScriptView
	:public View, public ICopyable, public IUndoable, public ISaveable
{
public:
	ScriptView(bool* show, const std::filesystem::path& filepath);
	~ScriptView() = default;

	virtual void OnAttach() override;
	virtual void OnDetach() override;
	virtual void OnImGuiRender() override;
	virtual void OnEvent(Event& event) override;

	virtual void Copy() override { m_TextEditor.Copy(); }
	virtual void Cut() override { m_TextEditor.Cut(); }
	virtual void Paste() override { m_TextEditor.Paste(); }
	virtual void Duplicate() override { m_TextEditor.Duplicate(); }
	virtual void Delete() override { m_TextEditor.Delete(); }

	virtual bool IsReadOnly() const override { return m_TextEditor.IsReadOnly(); }

	virtual bool CanUndo() const override { return m_TextEditor.CanUndo(); };
	virtual bool CanRedo() const override { return m_TextEditor.CanRedo(); };
	virtual void Undo(int asteps = 1) override { m_TextEditor.Undo(asteps); }
	virtual void Redo(int asteps = 1) override { m_TextEditor.Redo(asteps); }

	virtual bool HasSelection() const override { return m_TextEditor.HasSelection(); }
	virtual void SelectAll() override { m_TextEditor.SelectAll(); }

	virtual void Save() override;
	virtual void SaveAs() override;
	virtual bool NeedsSaving() override { return m_TextEditor.NeedsSaving(); }

	std::string GetText() const { return m_TextEditor.GetText(); }

	// Lines start at 1; a message also marks the line as an error
	void GoToLine(int line, const std::string& errorMessage = {}) { m_GoToLine = line; m_GoToLineError = errorMessage; }
private:
	TextEditor::LanguageDefinition DetermineLanguageDefinition();
	void ParseLuaScript();
	bool CompleteLua(const std::string& lineBefore, const std::string& word, std::vector<TextEditor::AutoCompleteItem>& items) const;
	bool CompleteLuaMember(const std::string& lineBefore, std::vector<TextEditor::AutoCompleteItem>& items) const;
	struct LuaStep { std::string name; bool call; };	// one name in a chain such as CurrentEntity:GetName(), and whether it's called
	static bool ParseLuaChain(const std::string& text, size_t end, std::vector<LuaStep>& steps, size_t& begin);
	std::string ResolveLuaType(const std::vector<LuaStep>& steps, const std::vector<std::string>& lines, int line, int depth) const;
	std::string FindLuaVariableType(const std::string& name, const std::vector<std::string>& lines, int line, int depth) const;
	void FindNext();
	void FindPrevious();
private:
	bool* m_Show;

	std::filesystem::path m_FilePath;

	Ref<LuaScript> m_Script;

	TextEditor m_TextEditor;

	char m_SearchBuffer[256] = {0};
	bool m_ShowSearch = false;
	bool m_FocusSearch = false;
	int m_GoToLine = 0;
	std::string m_GoToLineError;

	std::unordered_map<std::string, std::vector<LuaApiEntry>> m_LuaMembers;	// by the type or table they belong to
	std::unordered_map<std::string, std::string> m_LuaGlobalTypes;			// e.g. CurrentScene -> Scene
	std::vector<TextEditor::AutoCompleteItem> m_LuaGlobalWords;			// keywords and names with no owner, e.g. CurrentEntity
	std::vector<TextEditor::AutoCompleteItem> m_LuaMemberWords;			// names that only follow a "." or ":"
};
