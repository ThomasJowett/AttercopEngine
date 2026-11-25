#pragma once

class ICopyable
{
public:
	virtual void Copy() = 0;
	virtual void Cut() = 0;
	virtual void Paste() = 0;
	virtual void Duplicate() = 0;
	virtual void Delete() = 0;
	virtual bool HasSelection() const = 0;
	virtual void SelectAll() = 0;
	// Only panels that select entities have these
	virtual void SelectNone() {}
	virtual void InvertSelection() {}

	virtual bool IsReadOnly() const = 0;
};
